/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
 
#include "UDeferredTask.h"

#include <LCommander.h>
#include <LListener.h>

#include "CWindowMenu.h"
#include "CNetscapeWindow.h"
#include <typeinfo>

CDeferredTaskManager* CDeferredTaskManager::sManager = nil;

//----------------------------------------------------------------------------------------
CDeferredTaskManager::CDeferredTaskManager()
//----------------------------------------------------------------------------------------
:	mQueueList(nil)
{
	sManager = this;
	StartIdling();
} // CDeferredTaskManager::CDeferredTaskManager

//----------------------------------------------------------------------------------------
CDeferredTaskManager::~CDeferredTaskManager()
//----------------------------------------------------------------------------------------
{
	sManager = nil;
} // CDeferredTaskManager::CDeferredTaskManager

//----------------------------------------------------------------------------------------
/*static*/ void CDeferredTaskManager::DoQuit(Int32 /*inSaveOption*/)
// Called from uapp.cp.  Allow any pending tasks to complete before quitting.
//----------------------------------------------------------------------------------------
{
	// Allow all the close tasks to finish.  When the last task is done, sManager should
	// be set to null.
	EventRecord stupidNullEvent = {0};
	for (int timeOutCounter = 0; sManager && timeOutCounter < 500; timeOutCounter++)
	{
		sManager->DoExecuteTasks(); // netlib might be needed.
		DevoteTimeToRepeaters(stupidNullEvent);
	}
	Assert_(!sManager); // ow! we ran 500 times and the manager is still alive!
} // CDeferredTaskManager::DoQuit

//----------------------------------------------------------------------------------------
/* static */ void CDeferredTaskManager::Post(CDeferredTask* inTask, LPane* inPane)
//----------------------------------------------------------------------------------------
{
	if (!inTask)
		return;
	try
	{
		// If this is the first post, we'll make a new manager
		if (!sManager)
			new CDeferredTaskManager;
		sManager->DoPost(inTask, inPane, false);
	}
	catch(...)
	{
	}
} // CDeferredTaskManager::Post

//----------------------------------------------------------------------------------------
/* static */ void CDeferredTaskManager::Post1(CDeferredTask* inTask, LPane* inPane)
//----------------------------------------------------------------------------------------
{
	if (!inTask)
		return;
	try
	{
		// If this is the first post, we'll make a new manager
		if (!sManager)
			new CDeferredTaskManager;
		sManager->DoPost(inTask, inPane, true);
	}
	catch(...)
	{
	}
} // CDeferredTaskManager::Post1

//----------------------------------------------------------------------------------------
/* static */ void CDeferredTaskManager::Remove(CDeferredTask*& inTask, LPane* inPane)
//----------------------------------------------------------------------------------------
{
	Assert_(sManager);
	if (sManager)
		sManager->DoRemove(inTask, inPane);
	// Task may already have executed and been removed.  Always zero the caller's stale
	//  pointer.
	inTask = nil;
} // CDeferredTaskManager::Remove

//----------------------------------------------------------------------------------------
/* static */ void CDeferredTaskManager::ClearQueue(LPane* inPane)
//----------------------------------------------------------------------------------------
{
	// If there's no sManager, then there are no queues, so there's nothing to do.
	if (!sManager)
		return;
	sManager->DoClearQueue(inPane);
} // CDeferredTaskManager::Remove

//----------------------------------------------------------------------------------------
void CDeferredTaskManager::DoPost(CDeferredTask* inTask, LPane* inPane, Boolean inUnique)
//----------------------------------------------------------------------------------------
{
	LWindow* window = nil;
	if (inPane)
		window = LWindow::FetchWindowObject(inPane->GetMacPort());
	
	// Try to find a queue matching this window:
	CDeferredTaskQueue* q = mQueueList;
	while (q)
	{
		if (q->mQueueWindow == window)
			break;
		q = q->mNext;
	}
	if (!q)
	{
		// no queue for this window yet.  Make one, insert it at front of list.
		try
		{
			q = new CDeferredTaskQueue(window);
			q->mNext = mQueueList;
			mQueueList = q;
		}
		catch(...)
		{
			return;
		}
	}
	// Got a queue.  Post the task to it.  If it's a unique ("Post1") operation, remove
	// all tasks of the same type first.
	if (inUnique)
		q->DoRemoveType(inTask);
	q->DoPost(inTask);
} // CDeferredTaskManager::DoPost

//----------------------------------------------------------------------------------------
void CDeferredTaskManager::DoRemove(CDeferredTask*& inTask, LPane* inPane)
//----------------------------------------------------------------------------------------
{
	if (!inTask)
		return;
	LWindow* window = nil;
	if (inPane)
		window = LWindow::FetchWindowObject(inPane->GetMacPort());
	if (window)
		DoRemove(inTask, window);
}

//----------------------------------------------------------------------------------------
void CDeferredTaskManager::DoRemove(CDeferredTask*& inTask, LWindow* inWindow)
// A nil window is OK (and represents the global "nil" queue).
//----------------------------------------------------------------------------------------
{
	if (!inTask)
		return;

	// Try to find a queue matching the specified window.
	// Note to those who follow: the window pointed to by inWindow may be deleted, but
	// the pointer is still usable as an identifier for the task's queue.
	CDeferredTaskQueue* q = mQueueList;
	CDeferredTaskQueue* prev = nil;
	while (q)
	{
		if (q->mQueueWindow == inWindow)
		{
			q->DoRemove(inTask);
			break;
		}
		prev = q;
		q = q->mNext;
	}

	// If the queue for this window is empty, delete it from the queue list.
	if (q && q->mFrontTask == nil)
		RemoveNextQueueAfter(prev);
	
	// If there are no more queues, die.  Life is not worth living if there are no more
	// queues for me to manage.
	if (!mQueueList)
		delete this;
} // CDeferredTaskManager::DoRemove

//----------------------------------------------------------------------------------------
void CDeferredTaskManager::RemoveNextQueueAfter(CDeferredTaskQueue* inPreviousQueue)
// Remove the queue following the inPreviousQueue.  If inPreviousQueue is nil, remove
// first queue.
//----------------------------------------------------------------------------------------
{
	CDeferredTaskQueue* q = inPreviousQueue ? inPreviousQueue->mNext : mQueueList;
	CDeferredTaskQueue* next = q->mNext;
	delete q;
	if (inPreviousQueue)
		inPreviousQueue->mNext = next;
	else
		mQueueList = next;
} // CDeferredTaskManager::RemoveNextQueueAfter

//----------------------------------------------------------------------------------------
void CDeferredTaskManager::DoClearQueue(LPane* inPane)
//----------------------------------------------------------------------------------------
{
	LWindow* window = nil;
	if (inPane)
		window = LWindow::FetchWindowObject(inPane->GetMacPort());
	if (window)
		DoClearQueue(window);
} // CDeferredTaskManager::DoClearQueue

//----------------------------------------------------------------------------------------
void CDeferredTaskManager::DoClearQueue(LWindow* inWindow)
// A nil window is OK (and represents the global "nil" queue).
//----------------------------------------------------------------------------------------
{
	// Try to find a queue matching the specified window.
	// Note to those who follow: the window pointed to by inWindow may be deleted, but
	// the pointer is still usable as an identifier for the task's queue.
	CDeferredTaskQueue* q = mQueueList;
	CDeferredTaskQueue* prev = nil;
	while (q)
	{
		if (q->mQueueWindow == inWindow)
		{
			q->DoClearSelf();
			break;
		}
		prev = q;
		q = q->mNext;
	}

	// If the queue for this window is empty, delete it from the queue list.
	if (q && q->mFrontTask == nil)
		RemoveNextQueueAfter(prev);
	
	// If there are no more queues, die.  Life is not worth living if there are no more
	// queues for me to manage.
	if (!mQueueList)
		delete this;
} // CDeferredTaskManager::DoClearQueue

//----------------------------------------------------------------------------------------
void CDeferredTaskManager::DoExecuteTasks()
//----------------------------------------------------------------------------------------
{
	// Try to execute the first task in each window's queue.
	CDeferredTaskQueue* q = mQueueList;
	while (q)
	{
		CDeferredTask* frontTask = q->mFrontTask;
		CDeferredTaskQueue* nextQueue = q->mNext;
		if (frontTask)
		{
			CDeferredTask::ExecuteResult result = frontTask->Execute();
			if (result == CDeferredTask::eDoneDelete)
			{
				DoRemove(frontTask, q->mQueueWindow);
				// Note to those who follow: the window may be deleted here, but
				// the pointer is still usable as an identifier for the task's queue.
			}
			else if (result == CDeferredTask::eWaitStepBack)
			{
				// front task didn't complete its work, and wants to yield to the second
				// task (if any) on the next go round.
				CDeferredTask* nextTask = frontTask->mNext;
				if (nextTask)
				{
					q->mFrontTask = nextTask;
					frontTask->mNext = nextTask->mNext;
					nextTask->mNext = frontTask;
				}
			}
			// The third case (eWaitStayFront) is handled by just passing on to the next
			// queue.
		}
		q = nextQueue;
	}
} // CDeferredTaskManager::ExecuteTasks

//----------------------------------------------------------------------------------------
void CDeferredTaskManager::SpendTime(const EventRecord&)
//----------------------------------------------------------------------------------------
{
	DoExecuteTasks();	
} // CDeferredTaskManager::SpendTime

#pragma mark -

//----------------------------------------------------------------------------------------
CDeferredTaskQueue::CDeferredTaskQueue(LWindow* inWindow)
// A nil window is OK (and represents the global "nil" queue).
//----------------------------------------------------------------------------------------
:	mFrontTask(nil)
,	mQueueWindow(inWindow)
,	mNext(nil)
{
} // CDeferredTaskQueue::CDeferredTaskQueue

//----------------------------------------------------------------------------------------
CDeferredTaskQueue::~CDeferredTaskQueue()
//----------------------------------------------------------------------------------------
{
} // CDeferredTaskQueue::CDeferredTaskQueue

//----------------------------------------------------------------------------------------
void CDeferredTaskQueue::DoPost(CDeferredTask* inTask)
//----------------------------------------------------------------------------------------
{
	CDeferredTask* cur = mFrontTask;
	CDeferredTask* prev = nil;
	while (cur)
	{
		prev = cur;
		cur = cur->mNext;
	}
	if (prev)
		prev->mNext = inTask;
	else
		mFrontTask = inTask;
} // CDeferredTaskQueue::DoPost

//----------------------------------------------------------------------------------------
void CDeferredTaskQueue::DoRemove(CDeferredTask*& inTask)
//----------------------------------------------------------------------------------------
{
	if (!inTask)
		return;
	CDeferredTask* cur = mFrontTask;
	CDeferredTask* prev = nil;
	while (cur)
	{
		if (cur == inTask)
		{
			// Found the task in the queue, remove it.
			CDeferredTask* next = inTask->mNext;
			delete inTask;
			inTask = nil;	// that's why it's a reference.
							// Note that possibly inTask == mFrontTask
			if (prev)
				prev->mNext = next;
			else
				mFrontTask = next;
			break;
		}
		prev = cur;
		cur = cur->mNext;
	}
} // CDeferredTaskQueue::DoRemove

//----------------------------------------------------------------------------------------
void CDeferredTaskQueue::DoRemoveType(CDeferredTask* inTask)
// Remove all tasks whose class type is the same as inTask.  Used in Post1() calls before
// posting the new class.
//----------------------------------------------------------------------------------------
{
	if (!inTask)
		return;
	CDeferredTask* cur = mFrontTask;
	CDeferredTask* prev = nil;
	while (cur)
	{
		CDeferredTask* next = cur->mNext;
		if (typeid(*cur) == typeid(*inTask))
		{
			// Found a matching task in the queue, remove it.
			delete cur;
			if (prev)
				prev->mNext = next;
			else
				mFrontTask = next;
		}
		else
		{
			prev = cur;
		}
		cur = next;
	}
} // CDeferredTaskQueue::DoRemove

//----------------------------------------------------------------------------------------
void CDeferredTaskQueue::DoClearSelf()
// Remove all tasks whose class type is the same as inTask.  Used in Post1() calls before
// posting the new class.
//----------------------------------------------------------------------------------------
{
	while (mFrontTask)
	{
		CDeferredTask* dead = mFrontTask;
		mFrontTask = dead->mNext;
		delete dead;
	}
} // CDeferredTaskQueue::ClearSelf

#pragma mark -

//----------------------------------------------------------------------------------------
CDeferredTask::CDeferredTask()
//----------------------------------------------------------------------------------------
:	mNext(nil)
,	mExecuting(false)
#if DEBUG
,	mExecuteAttemptCount(0)
#endif
{
} // CDeferredTask::CDeferredTask

//----------------------------------------------------------------------------------------
CDeferredTask::~CDeferredTask()
//----------------------------------------------------------------------------------------
{
} // CDeferredTask::~CDeferredTask

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredTask::Execute()
//----------------------------------------------------------------------------------------
{
	if (mExecuting)
		return eWaitStayFront;
#if DEBUG
	mExecuteAttemptCount++;
	Assert_((mExecuteAttemptCount & 0x000003FF) != 0); // assert every 1024 attempts.
#endif
	mExecuting = true;
	ExecuteResult result = eWaitStayFront;
	try
	{
		result = ExecuteSelf();
	}
	catch(...)
	{
		result = eDoneDelete; // if threw exception, delete task so it won't execute again.
	}
	mExecuting = false;
	return result;
} // CDeferredTask::Execute

#pragma mark -

//----------------------------------------------------------------------------------------
CDeferredCommand::CDeferredCommand(
	LCommander* inCommander,
	CommandT	inCommand,
	void*		ioParam)
//----------------------------------------------------------------------------------------
:	mCommander(inCommander)
,	mCommand(inCommand)
,	mParam(ioParam)
{
} // CDeferredCommand::CDeferredCommand

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredCommand::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	if (mCommander && !mCommander->ObeyCommand(mCommand, mParam))
		return eWaitStayFront;
	return eDoneDelete;
} // CDeferredCommand::ExecuteSelf

#pragma mark -

//----------------------------------------------------------------------------------------
CDeferredMessage::CDeferredMessage(
	LListener*	inListener,
	MessageT	inMessage,
	void*		ioParam)
//----------------------------------------------------------------------------------------
:	mListener(inListener)
,	mMessage(inMessage)
,	mParam(ioParam)
{
} // CDeferredMessage::CDeferredMessage

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredMessage::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	if (mListener)
		mListener->ListenToMessage(mMessage, mParam);
	return eDoneDelete;
} // CDeferredMessage::ExecuteSelf

#pragma mark -

//----------------------------------------------------------------------------------------
CDeferredCloseTask::CDeferredCloseTask(
	LPane*				inPane)
//----------------------------------------------------------------------------------------
:	mWindow(nil)
{
	if (inPane)
		mWindow = dynamic_cast<CNetscapeWindow*>
			(LWindow::FetchWindowObject(inPane->GetMacPort()));
	if (mWindow)
	{
		mWindow->StopAllContexts(); // This can reshow the window, so do it first.
		mWindow->Hide();
		// Make sure there are no other load tasks and such.
		CDeferredTaskManager::ClearQueue(mWindow);
	}
} // CDeferredCloseTask::CDeferredCloseTask

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredCloseTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	if (!mWindow)
		return eDoneDelete;
	// Allow the double-click timer in the thread window to time out and delete itself
	// probably unnecessary to wait - safer?
	if (mWindow->ClickTimesAreClose(::TickCount()))
		return eWaitStayFront;
	// Wait till any pending URLs are finished
	if (mWindow->IsAnyContextBusy())
		return eWaitStayFront;
	if (mWindow)
		mWindow->DoClose();
	return eDoneDelete;
} // CDeferredCloseTask::ExecuteSelf

//----------------------------------------------------------------------------------------
/* static */ CDeferredCloseTask* CDeferredCloseTask::DeferredClose(LPane* inPane)
//----------------------------------------------------------------------------------------
{
	CDeferredCloseTask* task = new CDeferredCloseTask(inPane);
	CDeferredTaskManager::Post(task, inPane);
	return task;
}

