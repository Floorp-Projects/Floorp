/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTaskQueue.h"
#include "nsThreadUtils.h"
#include "SharedThreadPool.h"

namespace mozilla {

MediaTaskQueue::MediaTaskQueue(TemporaryRef<SharedThreadPool> aPool)
  : mPool(aPool)
  , mQueueMonitor("MediaTaskQueue::Queue")
  , mIsRunning(false)
  , mIsShutdown(false)
  , mIsFlushing(false)
{
  MOZ_COUNT_CTOR(MediaTaskQueue);
}

MediaTaskQueue::~MediaTaskQueue()
{
  MonitorAutoLock mon(mQueueMonitor);
  MOZ_ASSERT(mIsShutdown);
  MOZ_COUNT_DTOR(MediaTaskQueue);
}

nsresult
MediaTaskQueue::Dispatch(TemporaryRef<nsIRunnable> aRunnable)
{
  MonitorAutoLock mon(mQueueMonitor);
  return DispatchLocked(aRunnable, AbortIfFlushing);
}

nsresult
MediaTaskQueue::DispatchLocked(TemporaryRef<nsIRunnable> aRunnable,
                               DispatchMode aMode)
{
  mQueueMonitor.AssertCurrentThreadOwns();
  if (mIsFlushing && aMode == AbortIfFlushing) {
    return NS_ERROR_ABORT;
  }
  if (mIsShutdown) {
    return NS_ERROR_FAILURE;
  }
  mTasks.push(aRunnable);
  if (mIsRunning) {
    return NS_OK;
  }
  RefPtr<nsIRunnable> runner(new Runner(this));
  nsresult rv = mPool->Dispatch(runner, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch runnable to run MediaTaskQueue");
    return rv;
  }
  mIsRunning = true;

  return NS_OK;
}

class MediaTaskQueueSyncRunnable : public nsRunnable {
public:
  MediaTaskQueueSyncRunnable(TemporaryRef<nsIRunnable> aRunnable)
    : mRunnable(aRunnable)
    , mMonitor("MediaTaskQueueSyncRunnable")
    , mDone(false)
  {
  }

  NS_IMETHOD Run() {
    nsresult rv = mRunnable->Run();
    {
      MonitorAutoLock mon(mMonitor);
      mDone = true;
      mon.NotifyAll();
    }
    return rv;
  }

  nsresult WaitUntilDone() {
    MonitorAutoLock mon(mMonitor);
    while (!mDone) {
      mon.Wait();
    }
    return NS_OK;
  }
private:
  RefPtr<nsIRunnable> mRunnable;
  Monitor mMonitor;
  bool mDone;
};

nsresult
MediaTaskQueue::SyncDispatch(TemporaryRef<nsIRunnable> aRunnable) {
  RefPtr<MediaTaskQueueSyncRunnable> task(new MediaTaskQueueSyncRunnable(aRunnable));
  nsresult rv = Dispatch(task);
  NS_ENSURE_SUCCESS(rv, rv);
  return task->WaitUntilDone();
}

void
MediaTaskQueue::AwaitIdle()
{
  MonitorAutoLock mon(mQueueMonitor);
  AwaitIdleLocked();
}

void
MediaTaskQueue::AwaitIdleLocked()
{
  mQueueMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(mIsRunning || mTasks.empty());
  while (mIsRunning) {
    mQueueMonitor.Wait();
  }
}

void
MediaTaskQueue::Shutdown()
{
  MonitorAutoLock mon(mQueueMonitor);
  mIsShutdown = true;
  AwaitIdleLocked();
}

nsresult
MediaTaskQueue::FlushAndDispatch(TemporaryRef<nsIRunnable> aRunnable)
{
  MonitorAutoLock mon(mQueueMonitor);
  AutoSetFlushing autoFlush(this);
  while (!mTasks.empty()) {
    mTasks.pop();
  }
  nsresult rv = DispatchLocked(aRunnable, IgnoreFlushing);
  NS_ENSURE_SUCCESS(rv, rv);
  AwaitIdleLocked();
  return NS_OK;
}

void
MediaTaskQueue::Flush()
{
  MonitorAutoLock mon(mQueueMonitor);
  AutoSetFlushing autoFlush(this);
  while (!mTasks.empty()) {
    mTasks.pop();
  }
  AwaitIdleLocked();
}

bool
MediaTaskQueue::IsEmpty()
{
  MonitorAutoLock mon(mQueueMonitor);
  return mTasks.empty();
}

bool
MediaTaskQueue::IsCurrentThreadIn()
{
#ifdef DEBUG
  MonitorAutoLock mon(mQueueMonitor);
  return NS_GetCurrentThread() == mRunningThread;
#else
  return false;
#endif
}

nsresult
MediaTaskQueue::Runner::Run()
{
  RefPtr<nsIRunnable> event;
  {
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    MOZ_ASSERT(mQueue->mIsRunning);
    mQueue->mRunningThread = NS_GetCurrentThread();
    if (mQueue->mTasks.size() == 0) {
      mQueue->mIsRunning = false;
      mon.NotifyAll();
      return NS_OK;
    }
    event = mQueue->mTasks.front();
    mQueue->mTasks.pop();
  }
  MOZ_ASSERT(event);

  // Note that dropping the queue monitor before running the task, and
  // taking the monitor again after the task has run ensures we have memory
  // fences enforced. This means that if the object we're calling wasn't
  // designed to be threadsafe, it will be, provided we're only calling it
  // in this task queue.
  event->Run();

  // Drop the reference to event. The event will hold a reference to the
  // object it's calling, and we don't want to keep it alive, it may be
  // making assumptions what holds references to it. This is especially
  // the case if the object is waiting for us to shutdown, so that it
  // can shutdown (like in the MediaDecoderStateMachine's SHUTDOWN case).
  event = nullptr;

  {
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    if (mQueue->mTasks.size() == 0) {
      // No more events to run. Exit the task runner.
      mQueue->mIsRunning = false;
      mon.NotifyAll();
      mQueue->mRunningThread = nullptr;
      return NS_OK;
    }
  }

  // There's at least one more event that we can run. Dispatch this Runner
  // to the thread pool again to ensure it runs again. Note that we don't just
  // run in a loop here so that we don't hog the thread pool. This means we may
  // run on another thread next time, but we rely on the memory fences from
  // mQueueMonitor for thread safety of non-threadsafe tasks.
  {
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    // Note: Hold the monitor *before* we dispatch, in case we context switch
    // to another thread pool in the queue immediately and take the lock in the
    // other thread; mRunningThread could be set to the new thread's value and
    // then incorrectly anulled below in that case.
    nsresult rv = mQueue->mPool->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      // Failed to dispatch, shutdown!
      mQueue->mIsRunning = false;
      mQueue->mIsShutdown = true;
      mon.NotifyAll();
    }
    mQueue->mRunningThread = nullptr;
  }

  return NS_OK;
}

} // namespace mozilla
