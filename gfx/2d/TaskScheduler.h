/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TASKSCHEDULER_H_
#define MOZILLA_GFX_TASKSCHEDULER_H_

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Types.h"

#ifdef WIN32
#include "mozilla/gfx/TaskScheduler_win32.h"
#else
#include "mozilla/gfx/TaskScheduler_posix.h"
#endif

#include <vector>

namespace mozilla {
namespace gfx {

class MultiThreadedTaskQueue;
class SyncObject;
class WorkerThread;

class TaskScheduler {
public:
  /// Return one of the queues that the drawing worker threads pull from, chosen
  /// pseudo-randomly.
  static MultiThreadedTaskQueue* GetDrawingQueue()
  {
    return sSingleton->mDrawingQueues[
      sSingleton->mNextQueue++ % sSingleton->mDrawingQueues.size()
    ];
  }

  /// Return one of the queues that the drawing worker threads pull from with a
  /// hash to choose the queue.
  ///
  /// Calling this function several times with the same hash will yield the same queue.
  static MultiThreadedTaskQueue* GetDrawingQueue(uint32_t aHash)
  {
    return sSingleton->mDrawingQueues[
      aHash % sSingleton->mDrawingQueues.size()
    ];
  }

  /// Initialize the task scheduler with aNumThreads worker threads for drawing
  /// and aNumQueues task queues.
  ///
  /// The number of threads must be superior or equal to the number of queues
  /// (since for now a worker thread only pulls from one queue).
  static bool Init(uint32_t aNumThreads, uint32_t aNumQueues);

  /// Shut the scheduler down.
  ///
  /// This will block until worker threads are joined and deleted.
  static void ShutDown();

  /// Returns true if there is a successfully initialized TaskScheduler singleton.
  static bool IsEnabled() { return !!sSingleton; }

  /// Submit a task buffer to its associated queue.
  ///
  /// The caller looses ownership of the task buffer.
  static void SubmitTask(Task* aTasks);

  /// Process commands until the command buffer needs to block on a sync object,
  /// completes, yields, or encounters an error.
  ///
  /// Can be used on any thread. Worker threads basically loop over this, but the
  /// main thread can also dequeue pending task buffers and process them alongside
  /// the worker threads if it is about to block until completion anyway.
  ///
  /// The caller looses ownership of the task buffer.
  static TaskStatus ProcessTask(Task* aTasks);

protected:
  static TaskScheduler* sSingleton;

  // queues of Task that are ready to be processed
  std::vector<MultiThreadedTaskQueue*> mDrawingQueues;
  std::vector<WorkerThread*> mWorkerThreads;
  Atomic<uint32_t> mNextQueue;
};

/// Tasks are not reference-counted because they don't have shared ownership.
/// The ownership of tasks can change when they are passed to certain methods
/// of TaskScheduler and SyncObject. See the docuumentaion of these classes.
class Task {
public:
  Task(MultiThreadedTaskQueue* aQueue, SyncObject* aStart = nullptr, SyncObject* aCompletion = nullptr);

  virtual ~Task();

  virtual TaskStatus Run() = 0;

  /// For use in TaskScheduler::SubmitTask. Don't use it anywhere else.
  //already_AddRefed<SyncObject> GetAndResetStartSync();
  SyncObject* GetStartSync() { return mStartSync; }

  MultiThreadedTaskQueue* GetTaskQueue() { return mQueue; }

protected:

  MultiThreadedTaskQueue* mQueue;
  RefPtr<SyncObject> mStartSync;
  RefPtr<SyncObject> mCompletionSync;
};

class EventObject;

/// This task will set an EventObject.
///
/// Typically used as the final task, so that the main thread can block on the
/// corresponfing EventObject until all of the tasks are processed.
class SetEventTask : public Task
{
public:
  explicit SetEventTask(MultiThreadedTaskQueue* aQueue,
                        SyncObject* aStart = nullptr, SyncObject* aCompletion = nullptr);

  ~SetEventTask();

  TaskStatus Run() override;

  EventObject* GetEvent() { return mEvent; }

protected:
  RefPtr<EventObject> mEvent;
};

/// A synchronization object that can be used to express dependencies and ordering between
/// tasks.
///
/// Tasks can register to SyncObjects in order to asynchronously wait for a signal.
/// In practice, Task objects usually start with a sync object (startSyc) and end
/// with another one (completionSync).
/// a Task never gets processed before its startSync is in the signaled state, and
/// signals its completionSync as soon as it finishes. This is how dependencies
/// between tasks is expressed.
class SyncObject final : public external::AtomicRefCounted<SyncObject> {
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(SyncObject)

  /// Create a synchronization object.
  SyncObject();

  ~SyncObject();

  /// Attempt to register a task.
  ///
  /// If the sync object is already in the signaled state, the buffer is *not*
  /// registered and the sync object does not take ownership of the task.
  /// If the object is not yet in the signaled state, it takes ownership of
  /// the task and places it in a list of pending tasks.
  /// Pending tasks will not be processed by the worker thread.
  /// When the SyncObject reaches the signaled state, it places the pending
  /// tasks back in the available buffer queue, so that they can be
  /// scheduled again.
  ///
  /// Returns true if the SyncOject is not already in the signaled state.
  /// This means that if this method returns true, the SyncObject has taken
  /// ownership of the Task.
  bool Register(Task* aTask);

  /// Signal the SyncObject.
  ///
  /// This decrements an internal counter. The sync object reaches the signaled
  /// state when the counter gets to zero.
  /// calling Signal on a SyncObject that is already in the signaled state has
  /// no effect.
  void Signal();

  /// Returns true if mSignals is equal to zero. In other words, returns true
  /// if all subsequent tasks have already signaled the sync object.
  ///
  /// Note that this means SyncObject are initially in the signaled state, until
  /// a Task is created with and declares the sync objects as its "completion sync"
  bool IsSignaled();

private:
  // Called by Task's constructor
  void AddSubsequent(Task* aTask);
  void AddPrerequisite(Task* aTask);

  void AddWaitingTask(Task* aTask);

  void SubmitWaitingTasks();

#ifdef DEBUG
  // For debugging purposes only.
  std::vector<Task*> mPrerequisites;
  std::vector<Task*> mSubsequents;
#endif

  std::vector<Task*> mWaitingTasks;
  CriticalSection mWaitingTasksSection; // for concurrent access to mWaintingTasks
  Atomic<uint32_t> mSignals;
  Atomic<bool> mHasSubmittedSubsequent;

  friend class Task;
  friend class TaskScheduler;
};

/// Base class for worker threads.
class WorkerThread
{
public:
  static WorkerThread* Create(MultiThreadedTaskQueue* aTaskQueue);

  virtual ~WorkerThread() {}

  void Run();
protected:
  explicit WorkerThread(MultiThreadedTaskQueue* aTaskQueue);

  MultiThreadedTaskQueue* mQueue;
};

} // namespace
} // namespace

#endif