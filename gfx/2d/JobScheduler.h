/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TASKSCHEDULER_H_
#define MOZILLA_GFX_TASKSCHEDULER_H_

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/RefCounted.h"

#ifdef WIN32
#include "mozilla/gfx/JobScheduler_win32.h"
#else
#include "mozilla/gfx/JobScheduler_posix.h"
#endif

#include <vector>

namespace mozilla {
namespace gfx {

class MultiThreadedJobQueue;
class SyncObject;
class WorkerThread;

class JobScheduler {
public:
  /// Return one of the queues that the drawing worker threads pull from, chosen
  /// pseudo-randomly.
  static MultiThreadedJobQueue* GetDrawingQueue()
  {
    return sSingleton->mDrawingQueues[
      sSingleton->mNextQueue++ % sSingleton->mDrawingQueues.size()
    ];
  }

  /// Return one of the queues that the drawing worker threads pull from with a
  /// hash to choose the queue.
  ///
  /// Calling this function several times with the same hash will yield the same queue.
  static MultiThreadedJobQueue* GetDrawingQueue(uint32_t aHash)
  {
    return sSingleton->mDrawingQueues[
      aHash % sSingleton->mDrawingQueues.size()
    ];
  }

  /// Return the task queue associated to the worker the task is pinned to if
  /// the task is pinned to a worker, or a random queue.
  static MultiThreadedJobQueue* GetQueueForJob(Job* aJob);

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

  /// Returns true if there is a successfully initialized JobScheduler singleton.
  static bool IsEnabled() { return !!sSingleton; }

  /// Submit a task buffer to its associated queue.
  ///
  /// The caller looses ownership of the task buffer.
  static void SubmitJob(Job* aJobs);

  /// Process commands until the command buffer needs to block on a sync object,
  /// completes, yields, or encounters an error.
  ///
  /// Can be used on any thread. Worker threads basically loop over this, but the
  /// main thread can also dequeue pending task buffers and process them alongside
  /// the worker threads if it is about to block until completion anyway.
  ///
  /// The caller looses ownership of the task buffer.
  static JobStatus ProcessJob(Job* aJobs);

protected:
  static JobScheduler* sSingleton;

  // queues of Job that are ready to be processed
  std::vector<MultiThreadedJobQueue*> mDrawingQueues;
  std::vector<WorkerThread*> mWorkerThreads;
  Atomic<uint32_t> mNextQueue;
};

/// Jobs are not reference-counted because they don't have shared ownership.
/// The ownership of tasks can change when they are passed to certain methods
/// of JobScheduler and SyncObject. See the docuumentaion of these classes.
class Job {
public:
  Job(SyncObject* aStart, SyncObject* aCompletion, WorkerThread* aThread = nullptr);

  virtual ~Job();

  virtual JobStatus Run() = 0;

  /// For use in JobScheduler::SubmitJob. Don't use it anywhere else.
  //already_AddRefed<SyncObject> GetAndResetStartSync();
  SyncObject* GetStartSync() { return mStartSync; }

  bool IsPinnedToAThread() const { return !!mPinToThread; }

  WorkerThread* GetWorkerThread() { return mPinToThread; }

protected:
  // An intrusive linked list of tasks waiting for a sync object to enter the
  // signaled state. When the task is not waiting for a sync object, mNextWaitingJob
  // should be null. This is only accessed from the thread that owns the task.
  Job* mNextWaitingJob;

  RefPtr<SyncObject> mStartSync;
  RefPtr<SyncObject> mCompletionSync;
  WorkerThread* mPinToThread;

  friend class SyncObject;
};

class EventObject;

/// This task will set an EventObject.
///
/// Typically used as the final task, so that the main thread can block on the
/// corresponfing EventObject until all of the tasks are processed.
class SetEventJob : public Job
{
public:
  explicit SetEventJob(EventObject* aEvent,
                        SyncObject* aStart, SyncObject* aCompletion = nullptr,
                        WorkerThread* aPinToWorker = nullptr);

  ~SetEventJob();

  JobStatus Run() override;

  EventObject* GetEvent() { return mEvent; }

protected:
  RefPtr<EventObject> mEvent;
};

/// A synchronization object that can be used to express dependencies and ordering between
/// tasks.
///
/// Jobs can register to SyncObjects in order to asynchronously wait for a signal.
/// In practice, Job objects usually start with a sync object (startSyc) and end
/// with another one (completionSync).
/// a Job never gets processed before its startSync is in the signaled state, and
/// signals its completionSync as soon as it finishes. This is how dependencies
/// between tasks is expressed.
class SyncObject final : public external::AtomicRefCounted<SyncObject> {
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(SyncObject)

  /// Create a synchronization object.
  ///
  /// aNumPrerequisites represents the number of times the object must be signaled
  /// before actually entering the signaled state (in other words, it means the
  /// number of dependencies of this sync object).
  ///
  /// Explicitly specifying the number of prerequisites when creating sync objects
  /// makes it easy to start scheduling some of the prerequisite tasks while
  /// creating the others, which is how we typically use the task scheduler.
  /// Automatically determining the number of prerequisites using Job's constructor
  /// brings the risk that the sync object enters the signaled state while we
  /// are still adding prerequisites which is hard to fix without using muteces.
  explicit SyncObject(uint32_t aNumPrerequisites = 1);

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
  /// ownership of the Job.
  bool Register(Job* aJob);

  /// Signal the SyncObject.
  ///
  /// This decrements an internal counter. The sync object reaches the signaled
  /// state when the counter gets to zero.
  void Signal();

  /// Returns true if mSignals is equal to zero. In other words, returns true
  /// if all prerequisite tasks have already signaled the sync object.
  bool IsSignaled();

  /// Asserts that the number of added prerequisites is equal to the number
  /// specified in the constructor (does nothin in release builds).
  void FreezePrerequisites();

private:
  // Called by Job's constructor
  void AddSubsequent(Job* aJob);
  void AddPrerequisite(Job* aJob);

  void AddWaitingJob(Job* aJob);

  void SubmitWaitingJobs();

  Atomic<int32_t> mSignals;
  Atomic<Job*> mFirstWaitingJob;

#ifdef DEBUG
  uint32_t mNumPrerequisites;
  Atomic<uint32_t> mAddedPrerequisites;
#endif

  friend class Job;
  friend class JobScheduler;
};

/// Base class for worker threads.
class WorkerThread
{
public:
  static WorkerThread* Create(MultiThreadedJobQueue* aJobQueue);

  virtual ~WorkerThread() {}

  void Run();

  MultiThreadedJobQueue* GetJobQueue() { return mQueue; }

protected:
  explicit WorkerThread(MultiThreadedJobQueue* aJobQueue);

  virtual void SetName(const char* aName) {}

  MultiThreadedJobQueue* mQueue;
};

} // namespace
} // namespace

#endif
