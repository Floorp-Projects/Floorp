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
#include "nsThreadUtils.h"

class nsIRunnable;

namespace mozilla {

class SharedThreadPool;

// Abstracts executing runnables in order in a thread pool. The runnables
// dispatched to the MediaTaskQueue will be executed in the order in which
// they're received, and are guaranteed to not be executed concurrently.
// They may be executed on different threads, and a memory barrier is used
// to make this threadsafe for objects that aren't already threadsafe.
class MediaTaskQueue MOZ_FINAL {
  ~MediaTaskQueue();

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaTaskQueue)

  explicit MediaTaskQueue(TemporaryRef<SharedThreadPool> aPool);

  nsresult Dispatch(TemporaryRef<nsIRunnable> aRunnable);

  nsresult SyncDispatch(TemporaryRef<nsIRunnable> aRunnable);

  nsresult FlushAndDispatch(TemporaryRef<nsIRunnable> aRunnable);

  // Removes all pending tasks from the task queue, and blocks until
  // the currently running task (if any) finishes.
  void Flush();

  // Blocks until all tasks finish executing, then shuts down the task queue
  // and exits.
  void Shutdown();

  // Blocks until all task finish executing.
  void AwaitIdle();

  bool IsEmpty();

  // Returns true if the current thread is currently running a Runnable in
  // the task queue. This is for debugging/validation purposes only.
  bool IsCurrentThreadIn();

private:

  // Blocks until all task finish executing. Called internally by methods
  // that need to wait until the task queue is idle.
  // mQueueMonitor must be held.
  void AwaitIdleLocked();

  enum DispatchMode { AbortIfFlushing, IgnoreFlushing };

  nsresult DispatchLocked(TemporaryRef<nsIRunnable> aRunnable,
                          DispatchMode aMode);

  RefPtr<SharedThreadPool> mPool;

  // Monitor that protects the queue and mIsRunning;
  Monitor mQueueMonitor;

  // Queue of tasks to run.
  std::queue<RefPtr<nsIRunnable>> mTasks;

  // The thread currently running the task queue. We store a reference
  // to this so that IsCurrentThreadIn() can tell if the current thread
  // is the thread currently running in the task queue.
  RefPtr<nsIThread> mRunningThread;

  // True if we've dispatched an event to the pool to execute events from
  // the queue.
  bool mIsRunning;

  // True if we've started our shutdown process.
  bool mIsShutdown;

  class MOZ_STACK_CLASS AutoSetFlushing
  {
  public:
    explicit AutoSetFlushing(MediaTaskQueue* aTaskQueue) : mTaskQueue(aTaskQueue)
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
    MediaTaskQueue* mTaskQueue;
  };

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

} // namespace mozilla

#endif // MediaTaskQueue_h_
