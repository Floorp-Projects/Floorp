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
#include "mozilla/ThreadLocal.h"
#include "mozilla/unused.h"
#include "SharedThreadPool.h"
#include "nsThreadUtils.h"
#include "MediaPromise.h"
#include "TaskDispatcher.h"

class nsIRunnable;

namespace mozilla {

class SharedThreadPool;

typedef MediaPromise<bool, bool, false> ShutdownPromise;

// Abstracts executing runnables in order in a thread pool. The runnables
// dispatched to the MediaTaskQueue will be executed in the order in which
// they're received, and are guaranteed to not be executed concurrently.
// They may be executed on different threads, and a memory barrier is used
// to make this threadsafe for objects that aren't already threadsafe.
class MediaTaskQueue : public AbstractThread {
  static ThreadLocal<MediaTaskQueue*> sCurrentQueueTLS;
public:
  static void InitStatics();

  // Returns the task queue that the caller is currently running in, or null
  // if the caller is not running in a MediaTaskQueue.
  static MediaTaskQueue* GetCurrentQueue() { return sCurrentQueueTLS.get(); }

  explicit MediaTaskQueue(TemporaryRef<SharedThreadPool> aPool, bool aRequireTailDispatch = false);

  void Dispatch(TemporaryRef<nsIRunnable> aRunnable,
                DispatchFailureHandling aFailureHandling = AssertDispatchSuccess)
  {
    nsCOMPtr<nsIRunnable> r = dont_AddRef(aRunnable.take());
    return Dispatch(r.forget(), aFailureHandling);
  }

  // Returns a TaskDispatcher that will dispatch its tasks when the currently-
  // running tasks pops off the stack.
  //
  // May only be called when running within the task queue it is invoked up.
  TaskDispatcher& TailDispatcher();

  // Returns true if this task queue requires all dispatches performed by its
  // tasks to go through the tail dispatcher.
  bool RequiresTailDispatch() { return mRequireTailDispatch; }

#ifdef DEBUG
  static void AssertInTailDispatchIfNeeded()
  {
    // See if we're currently running in a task queue that asserts tail
    // dispatch.
    MediaTaskQueue* currentQueue = MediaTaskQueue::GetCurrentQueue();
    if (!currentQueue || !currentQueue->RequiresTailDispatch()) {
      return;
    }

    // This is a bit tricky. The only moment when we're running in a task queue
    // but don't have mTailDispatcher set is precisely the moment that we're
    // doing tail dispatch (i.e. when AutoTaskGuard's destructor has already
    // run and AutoTaskDispatcher's destructor is currently running).
    MOZ_ASSERT(!currentQueue->mTailDispatcher,
               "Not allowed to dispatch tasks directly from this task queue - use TailDispatcher()");
  }
#else
  static void AssertInTailDispatchIfNeeded() {}
#endif

  // For AbstractThread.
  void Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                DispatchFailureHandling aFailureHandling = AssertDispatchSuccess) override
  {
    MonitorAutoLock mon(mQueueMonitor);
    nsresult rv = DispatchLocked(Move(aRunnable), AbortIfFlushing);
    MOZ_DIAGNOSTIC_ASSERT(aFailureHandling == DontAssertDispatchSuccess || NS_SUCCEEDED(rv));
    unused << rv;
  }

  // DEPRECATED! Do not us, if a flush happens at the same time, this function
  // can hang and block forever!
  void SyncDispatch(TemporaryRef<nsIRunnable> aRunnable);

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
  // the task queue.
  bool IsCurrentThreadIn() override;

protected:
  virtual ~MediaTaskQueue();


  // Blocks until all task finish executing. Called internally by methods
  // that need to wait until the task queue is idle.
  // mQueueMonitor must be held.
  void AwaitIdleLocked();

  enum DispatchMode { AbortIfFlushing, IgnoreFlushing };

  nsresult DispatchLocked(already_AddRefed<nsIRunnable> aRunnable, DispatchMode aMode);

  RefPtr<SharedThreadPool> mPool;

  // Monitor that protects the queue and mIsRunning;
  Monitor mQueueMonitor;

  // Queue of tasks to run.
  std::queue<nsCOMPtr<nsIRunnable>> mTasks;

  // The thread currently running the task queue. We store a reference
  // to this so that IsCurrentThreadIn() can tell if the current thread
  // is the thread currently running in the task queue.
  //
  // This may be read on any thread, but may only be written on mRunningThread.
  // The thread can't die while we're running in it, and we only use it for
  // pointer-comparison with the current thread anyway - so we make it atomic
  // and don't refcount it.
  Atomic<nsIThread*> mRunningThread;

  // RAII class that gets instantiated for each dispatched task.
  class AutoTaskGuard : public AutoTaskDispatcher
  {
  public:
    explicit AutoTaskGuard(MediaTaskQueue* aQueue) : mQueue(aQueue)
    {
      // NB: We don't hold the lock to aQueue here. Don't do anything that
      // might require it.
      MOZ_ASSERT(!mQueue->mTailDispatcher);
      mQueue->mTailDispatcher = this;

      MOZ_ASSERT(sCurrentQueueTLS.get() == nullptr);
      sCurrentQueueTLS.set(aQueue);

      MOZ_ASSERT(mQueue->mRunningThread == nullptr);
      mQueue->mRunningThread = NS_GetCurrentThread();
    }

    ~AutoTaskGuard()
    {
      MOZ_ASSERT(mQueue->mRunningThread == NS_GetCurrentThread());
      mQueue->mRunningThread = nullptr;

      sCurrentQueueTLS.set(nullptr);
      mQueue->mTailDispatcher = nullptr;
    }

  private:
  MediaTaskQueue* mQueue;
  };

  TaskDispatcher* mTailDispatcher;

  // True if we've dispatched an event to the pool to execute events from
  // the queue.
  bool mIsRunning;

  // True if we've started our shutdown process.
  bool mIsShutdown;
  MediaPromiseHolder<ShutdownPromise> mShutdownPromise;

  // True if we're flushing; we reject new tasks if we're flushing.
  bool mIsFlushing;

  // True if we want to require that every task dispatched from tasks running in
  // this queue go through our queue's tail dispatcher.
  bool mRequireTailDispatch;

  class Runner : public nsRunnable {
  public:
    explicit Runner(MediaTaskQueue* aQueue)
      : mQueue(aQueue)
    {
    }
    NS_METHOD Run() override;
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

  bool IsDispatchReliable() override { return false; }

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
