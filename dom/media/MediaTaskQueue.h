/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaTaskQueue_h_
#define MediaTaskQueue_h_

#include <queue>
#include "mozilla/RefPtr.h"
#include "mozilla/Monitor.h"
#include "SharedThreadPool.h"
#include "nsThreadUtils.h"
#include "MediaPromise.h"

class nsIRunnable;

namespace mozilla {

class SharedThreadPool;

typedef MediaPromise<bool, bool, false> ShutdownPromise;

// Abstracts executing runnables in order in a thread pool. The runnables
// dispatched to the MediaTaskQueue will be executed in the order in which
// they're received, and are guaranteed to not be executed concurrently.
// They may be executed on different threads, and a memory barrier is used
// to make this threadsafe for objects that aren't already threadsafe.
class MediaTaskQueue {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaTaskQueue)

  explicit MediaTaskQueue(TemporaryRef<SharedThreadPool> aPool);

  nsresult Dispatch(TemporaryRef<nsIRunnable> aRunnable);

  // This should only be used for things that absolutely can't afford to be
  // flushed. Normal operations should use Dispatch.
  nsresult ForceDispatch(TemporaryRef<nsIRunnable> aRunnable);

  nsresult SyncDispatch(TemporaryRef<nsIRunnable> aRunnable);

  // Puts the queue in a shutdown state and returns immediately. The queue will
  // remain alive at least until all the events are drained, because the Runners
  // hold a strong reference to the task queue, and one of them is always held
  // by the threadpool event queue when the task queue is non-empty.
  //
  // The returned promise is resolved when the queue goes empty.
  nsRefPtr<ShutdownPromise> BeginShutdown();

  // Blocks until all task finish executing.
  void AwaitIdle();

  // Blocks until the queue is flagged for shutdown and all tasks have finished
  // executing.
  void AwaitShutdownAndIdle();

  bool IsEmpty();

  // Returns true if the current thread is currently running a Runnable in
  // the task queue. This is for debugging/validation purposes only.
  bool IsCurrentThreadIn();

protected:
  virtual ~MediaTaskQueue();


  // Blocks until all task finish executing. Called internally by methods
  // that need to wait until the task queue is idle.
  // mQueueMonitor must be held.
  void AwaitIdleLocked();

  enum DispatchMode { AbortIfFlushing, IgnoreFlushing, Forced };

  nsresult DispatchLocked(TemporaryRef<nsIRunnable> aRunnable,
                          DispatchMode aMode);

  RefPtr<SharedThreadPool> mPool;

  // Monitor that protects the queue and mIsRunning;
  Monitor mQueueMonitor;

  struct TaskQueueEntry {
    RefPtr<nsIRunnable> mRunnable;
    bool mForceDispatch;

    explicit TaskQueueEntry(TemporaryRef<nsIRunnable> aRunnable,
                            bool aForceDispatch = false)
      : mRunnable(aRunnable), mForceDispatch(aForceDispatch) {}
  };

  // Queue of tasks to run.
  std::queue<TaskQueueEntry> mTasks;

  // The thread currently running the task queue. We store a reference
  // to this so that IsCurrentThreadIn() can tell if the current thread
  // is the thread currently running in the task queue.
  RefPtr<nsIThread> mRunningThread;

  // True if we've dispatched an event to the pool to execute events from
  // the queue.
  bool mIsRunning;

  // True if we've started our shutdown process.
  bool mIsShutdown;
  MediaPromiseHolder<ShutdownPromise> mShutdownPromise;

  // True if we're flushing; we reject new tasks if we're flushing.
  bool mIsFlushing;

  class Runner : public nsRunnable {
  public:
    explicit Runner(MediaTaskQueue* aQueue)
      : mQueue(aQueue)
    {
    }
    NS_METHOD Run() MOZ_OVERRIDE;
  private:
    RefPtr<MediaTaskQueue> mQueue;
  };
};

class FlushableMediaTaskQueue : public MediaTaskQueue
{
public:
  explicit FlushableMediaTaskQueue(TemporaryRef<SharedThreadPool> aPool) : MediaTaskQueue(aPool) {}
  nsresult FlushAndDispatch(TemporaryRef<nsIRunnable> aRunnable);
  void Flush();

private:

  class MOZ_STACK_CLASS AutoSetFlushing
  {
  public:
    explicit AutoSetFlushing(FlushableMediaTaskQueue* aTaskQueue) : mTaskQueue(aTaskQueue)
    {
      mTaskQueue->mQueueMonitor.AssertCurrentThreadOwns();
      mTaskQueue->mIsFlushing = true;
    }
    ~AutoSetFlushing()
    {
      mTaskQueue->mQueueMonitor.AssertCurrentThreadOwns();
      mTaskQueue->mIsFlushing = false;
    }

  private:
    FlushableMediaTaskQueue* mTaskQueue;
  };

  void FlushLocked();

};

} // namespace mozilla

#endif // MediaTaskQueue_h_
