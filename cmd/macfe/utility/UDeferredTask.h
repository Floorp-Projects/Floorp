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
 
#pragma once

//========================================================================================
// This file contains a suite of classes that allow posting and deferred execution
// of tasks in any powerplant application (except for CDeferredCloseTask, which is
// specific to Communicator).

// CDeferredTaskManager:	A singleton class, manages each per-window queue of tasks,
//							together with one global "nil" queue. It is created only when
//							a task is first posted, and is deleted as soon as no more
//							tasks exist.
// CDeferredTaskQueue:		A queue of tasks.  Each task belongs to a window (except that
//							one global "nil" queue is allowed), and tasks for
//							different windows are in different queues.  A queue is created
//							only when needed and is deleted immediately it becomes empty.

// CDeferredTask:			The abstract base class for tasks that can be posted.
// CDeferredCommand			This is an example of a deferred task object.  Its ExecuteSelf()
//							method calls ObeyCommand
// CDeferredMessage			This is another example, but calls ListenToMessage instead.
// CDeferredCloseTask		Another example.  Closes a window when it's safe.
//========================================================================================

#include <LPeriodical.h>

class CDeferredTaskQueue; // forward.
class CDeferredTask; // forward.

//========================================================================================
class CDeferredTaskManager
//	On each idle, the task manager will walk the queue list.  For each queue, it will
//	take the following action:
//		Tell the first task to execute.  If the task returns true, the task is then
//		deleted. If it returns false, it is left in the queue to be tried on the next idle.
//	There are only a few public entry points: Post, Post1 and Remove.  Each of these has
//	an LPane* parameter that is used to work out which window, and hence which queue, the
//	task belongs to.  When posting, a new queue will be opened for the window in question,
//	if none exists.  When removing, the queue is deleted if no more tasks remain in it.
//========================================================================================
:	public LPeriodical
{
	public:		// ----------   BEGIN PUBLIC API ------------- //
		static void							Post(CDeferredTask* inTask, LPane* inPane);
												// Post a task to the end of the queue.
		static void							Post1(CDeferredTask* inTask, LPane* inPane);
												// Post a task to the end of the queue,
												// after removing all existing tasks of
												// the same type.
		static void							Remove(CDeferredTask*& inTask, LPane* inPane);
												// Remove  task.
		static void							ClearQueue(LPane* inPane);
												// Remove all tasks from this queue.
		static void							DoQuit(Int32 inSaveOption);
												// Finish all the close tasks.
				// ----------    END PUBLIC API  ------------- //
	protected:
											CDeferredTaskManager();
											~CDeferredTaskManager();
		virtual void						SpendTime(const EventRecord&);
		void								DoExecuteTasks();
		void								DoPost(
												CDeferredTask* inTask,
												LPane* inPane,
												Boolean inUnique);
		void								DoRemove(
												CDeferredTask*& inTask,
												LPane* inPane);
		void								DoRemove(
												CDeferredTask*& inTask,
												LWindow* inWindow);
		void								DoClearQueue(LPane* inPane);
		void								DoClearQueue(LWindow* inWindow);
		void								RemoveNextQueueAfter(
												CDeferredTaskQueue* inPreviousQueue);
//	data
	protected:
		CDeferredTaskQueue*					mQueueList;
		static CDeferredTaskManager*		sManager;
}; // class CDeferredTaskManager

//========================================================================================
class CDeferredTaskQueue
//	There is one queue per window, and optionally one global one (with mQueueWindow
//	== nil).  The first task in each queue "blocks" any subsequent tasks in the same queue.
//	FIFO order is thus guaranteed.
//========================================================================================
{
	friend class CDeferredTaskManager;
	protected:
											CDeferredTaskQueue(LWindow* inWindow);
											~CDeferredTaskQueue();
		void								DoPost(CDeferredTask* inTask);
		void								DoRemove(CDeferredTask*& inTask);
		void								DoRemoveType(CDeferredTask* inTask);
		void								DoClearSelf();
//	data
	protected:
		LWindow*							mQueueWindow;
		CDeferredTaskQueue*					mNext;
		CDeferredTask*						mFrontTask;
}; // class CDeferredWindowTaskQueue

//========================================================================================
class CDeferredTask
// Base class for various useful types of deferred tasks.  Derived classes must implement
// ExecuteSelf(), returning an ExecuteResult value of:
//	eWaitStayFront		task is to be tried again, and wishes to remain blocking the queue
//	eWaitDoneDelete		task has done its work and is to be deleted
//	eWaitStepBack		task is to be tried again, but willing to step back in the queue
//						to give the next task a chance. (background, low priority)
//========================================================================================
{
	protected:
		enum ExecuteResult { eWaitStayFront, eDoneDelete, eWaitStepBack };
											CDeferredTask();
		virtual								~CDeferredTask();
		virtual ExecuteResult				ExecuteSelf() = 0;
	private:
		ExecuteResult						Execute();
	// data
	protected:
		friend class CDeferredTaskManager;
		friend class CDeferredTaskQueue;
		CDeferredTask*						mNext;
		Boolean								mExecuting; // reentrancy protection.
#if DEBUG
		UInt32								mExecuteAttemptCount;
#endif
}; // class CDeferredTask

class LCommander;

//========================================================================================
class CDeferredCommand
// This is an example of a CDeferredTask.  This one calls ObeyCommand on a commander.
//========================================================================================
:	public CDeferredTask
{
	public:
											CDeferredCommand(
												LCommander* inCommander,
												CommandT	inCommand,
												void*		ioParam);
		virtual ExecuteResult					ExecuteSelf();
	// data
		LCommander*							mCommander;
		CommandT							mCommand;
		void*								mParam;
}; // class CDeferredCommand

class LListener;

//========================================================================================
class CDeferredMessage
// This is an example of a CDeferredTask.  This one calls ListenToMessage on a listener.
//========================================================================================
:	public CDeferredTask
{
	public:
											CDeferredMessage(
												LListener*	inListener,
												MessageT	inMessage,
												void*		ioParam);
		virtual ExecuteResult				ExecuteSelf();
	// data
		LListener*							mListener;
		MessageT							mMessage;
		void*								mParam;
}; // class CDeferredMessage

class CNetscapeWindow;

//========================================================================================
class CDeferredCloseTask
// This guy closes a window at a later, safer time.  The parameter to the constructor is
// any pane (usually the calling pane), from which the window object is deduced.  This
// only works for CNetscapeWindows
//========================================================================================
:	public CDeferredTask
{
	public:
											CDeferredCloseTask(LPane* inPane);
		static CDeferredCloseTask*			DeferredClose(LPane* inPane);
	protected:
		virtual ExecuteResult				ExecuteSelf();
// data
	protected:
		CNetscapeWindow*					mWindow;
}; // class CDeferredCloseTask
