// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextThreadToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "scheduler.h"
#include "system.h"

//----------------------------------------------------------------------
// NachOSscheduler::NachOSscheduler
// 	Initialize the list of ready but not running threads to empty.
//  Initialize
//----------------------------------------------------------------------

NachOSscheduler::NachOSscheduler()
{
    readyThreadList = new List;
    sleepingThreadList = new List;
}

//----------------------------------------------------------------------
// NachOSscheduler::~NachOSscheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

NachOSscheduler::~NachOSscheduler()
{
    delete readyThreadList;
    delete sleepingThreadList;
}

//----------------------------------------------------------------------
// NachOSscheduler::ThreadIsReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
NachOSscheduler::ThreadIsReadyToRun (NachOSThread *thread)
{
    DEBUG('t', "Putting thread %s on ready list.\n", thread->getName());

    thread->setStatus(READY);
    readyThreadList->Append((void *)thread);
}

//----------------------------------------------------------------------
// NachOSscheduler::FindNextThreadToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

NachOSThread *
NachOSscheduler::FindNextThreadToRun ()
{
    return (NachOSThread *)readyThreadList->Remove();
}

//----------------------------------------------------------------------
// NachOSscheduler::InsertSleepingThread
//	Put the thread on the sleeping thread list
//
//	"thread" is the thread to be put on the sleeping thread list.
//----------------------------------------------------------------------
void
NachOSscheduler::InsertSleepingThread(NachOSThread *thread)
{
    DEBUG('t', "Putting thread %s on sleeping list.\n",thread->getName());
    sleepingThreadList->Append((void *)thread);
}

//---------------------------------------------------------------------
// NachOSscheduler::RemoveSleepingThread
//	"thread" is the thread to be put on the sleeping thread list.
//	remove the argument thread from the sleeping thread list and return it
//  return NULL if argument thread is not present in sleepingThreadList
//---------------------------------------------------------------------
NachOSThread*
NachOSscheduler::RemoveSleepingThread(NachOSThread *thread)
{
    NachOSThread *start;
    NachOSThread *current;

    //remove a thread from sleepingThreadList and check if its pid matches
    //with the pid of thread to be removed

    current = (NachOSThread*)sleepingThreadList->Remove();
    start = current;
    if(current->getPID() == thread->getPID())   return current;

    //if pid of the two does not match just append the removed thread and
    //iteratively check for threads one by one in the list
    sleepingThreadList->Append((void *)current);
    do
    {
        current = (NachOSThread*)sleepingThreadList->Remove();
        //check pid of removed thread
        if(current->getPID() == thread->getPID())
	       return current;
        sleepingThreadList->Append((void *)current);
    }
    while(start->getPID() != current->getPID());
    return NULL;
}

//----------------------------------------------------------------------
// NachOSscheduler::IsSleepingListEmpty
//  checks if the SleepingListEmpty is empty or not
//----------------------------------------------------------------------
bool
NachOSscheduler::IsSleepingListEmpty()
{
    return sleepingThreadList->IsEmpty();
}
//----------------------------------------------------------------------
// NachOSscheduler::Schedule
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//----------------------------------------------------------------------

void
NachOSscheduler::Schedule (NachOSThread *nextThread)
{
    NachOSThread *oldThread = currentThread;

#ifdef USER_PROGRAM			// ignore until running user programs
    if (currentThread->space != NULL) {	// if this thread is a user program,
        currentThread->SaveUserState(); // save the user's CPU registers
	currentThread->space->SaveStateOnSwitch();
    }
#endif

    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    currentThread = nextThread;		    // switch to the next thread
    currentThread->setStatus(RUNNING);      // nextThread is now running

    DEBUG('t', "Switching from thread \"%s\" to thread \"%s\"\n",
	  oldThread->getName(), nextThread->getName());

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    _SWITCH(oldThread, nextThread);

    DEBUG('t', "Now in thread \"%s\"\n", currentThread->getName());

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in NachOSThread::FinishThread()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	       threadToBeDestroyed = NULL;
    }

#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
	currentThread->space->RestoreStateOnSwitch();
    }
#endif
}

//----------------------------------------------------------------------
// NachOSscheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
NachOSscheduler::Print()
{
    printf("Ready list contents:\n");
    readyThreadList->Mapcar((VoidFunctionPtr) ThreadPrint);
}

