/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTaskQueue.h"
#include "nsThreadUtils.h"
#include "SharedThreadPool.h"

namespace mozilla {

ThreadLocal<MediaTaskQueue*> MediaTaskQueue::sCurrentQueueTLS;

/* static */ void
MediaTaskQueue::InitStatics()
{
  if (!sCurrentQueueTLS.init()) {
    MOZ_CRASH();
  }
}

MediaTaskQueue::MediaTaskQueue(TemporaryRef<SharedThreadPool> aPool,
                               bool aRequireTailDispatch)
  : mPool(aPool)
  , mQueueMonitor("MediaTaskQueue::Queue")
  , mTailDispatcher(nullptr)
  , mIsRunning(false)
  , mIsShutdown(false)
  , mIsFlushing(false)
  , mRequireTailDispatch(aRequireTailDispatch)
{
  MOZ_COUNT_CTOR(MediaTaskQueue);
}

MediaTaskQueue::~MediaTaskQueue()
{
  MonitorAutoLock mon(mQueueMonitor);
  MOZ_ASSERT(mIsShutdown);
  MOZ_COUNT_DTOR(MediaTaskQueue);
}

TaskDispatcher&
MediaTaskQueue::TailDispatcher()
{
  MOZ_ASSERT(IsCurrentThreadIn());
  MOZ_ASSERT(mTailDispatcher);
  return *mTailDispatcher;
}

nsresult
MediaTaskQueue::DispatchLocked(already_AddRefed<nsIRunnable> aRunnable, DispatchMode aMode)
{
  mQueueMonitor.AssertCurrentThreadOwns();
  nsCOMPtr<nsIRunnable> r = aRunnable;
  if (mIsFlushing && aMode == AbortIfFlushing) {
    return NS_ERROR_ABORT;
  }
  if (mIsShutdown) {
    return NS_ERROR_FAILURE;
  }
  mTasks.push(r.forget());
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
  explicit MediaTaskQueueSyncRunnable(TemporaryRef<nsIRunnable> aRunnable)
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

  void WaitUntilDone() {
    MonitorAutoLock mon(mMonitor);
    while (!mDone) {
      mon.Wait();
    }
  }
private:
  RefPtr<nsIRunnable> mRunnable;
  Monitor mMonitor;
  bool mDone;
};

void
MediaTaskQueue::SyncDispatch(TemporaryRef<nsIRunnable> aRunnable) {
  NS_WARNING("MediaTaskQueue::SyncDispatch is dangerous and deprecated. Stop using this!");
  RefPtr<MediaTaskQueueSyncRunnable> task(new MediaTaskQueueSyncRunnable(aRunnable));
  Dispatch(task);
  task->WaitUntilDone();
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
MediaTaskQueue::AwaitShutdownAndIdle()
{
  MonitorAutoLock mon(mQueueMonitor);
  while (!mIsShutdown) {
    mQueueMonitor.Wait();
  }
  AwaitIdleLocked();
}

nsRefPtr<ShutdownPromise>
MediaTaskQueue::BeginShutdown()
{
  MonitorAutoLock mon(mQueueMonitor);
  mIsShutdown = true;
  nsRefPtr<ShutdownPromise> p = mShutdownPromise.Ensure(__func__);
  if (!mIsRunning) {
    mShutdownPromise.Resolve(true, __func__);
  }
  mon.NotifyAll();
  return p;
}

void
FlushableMediaTaskQueue::Flush()
{
  MonitorAutoLock mon(mQueueMonitor);
  AutoSetFlushing autoFlush(this);
  FlushLocked();
  AwaitIdleLocked();
}

nsresult
FlushableMediaTaskQueue::FlushAndDispatch(TemporaryRef<nsIRunnable> aRunnable)
{
  AssertInTailDispatchIfNeeded(); // Do this before acquiring the monitor.
  MonitorAutoLock mon(mQueueMonitor);
  AutoSetFlushing autoFlush(this);
  FlushLocked();
  nsCOMPtr<nsIRunnable> r = dont_AddRef(aRunnable.take());
  nsresult rv = DispatchLocked(r.forget(), IgnoreFlushing);
  NS_ENSURE_SUCCESS(rv, rv);
  AwaitIdleLocked();
  return NS_OK;
}

void
FlushableMediaTaskQueue::FlushLocked()
{
  mQueueMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(mIsFlushing);

  // Clear the tasks. If this strikes you as awful, stop using a
  // FlushableMediaTaskQueue.
  while (!mTasks.empty()) {
    mTasks.pop();
  }
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
  bool in = NS_GetCurrentThread() == mRunningThread;
  MOZ_ASSERT_IF(in, GetCurrentQueue() == this);
  return in;
}

nsresult
MediaTaskQueue::Runner::Run()
{
  RefPtr<nsIRunnable> event;
  {
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    MOZ_ASSERT(mQueue->mIsRunning);
    if (mQueue->mTasks.size() == 0) {
      mQueue->mIsRunning = false;
      mQueue->mShutdownPromise.ResolveIfExists(true, __func__);
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
  {
    AutoTaskGuard g(mQueue);
    event->Run();
  }

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
      mQueue->mShutdownPromise.ResolveIfExists(true, __func__);
      mon.NotifyAll();
      return NS_OK;
    }
  }

  // There's at least one more event that we can run. Dispatch this Runner
  // to the thread pool again to ensure it runs again. Note that we don't just
  // run in a loop here so that we don't hog the thread pool. This means we may
  // run on another thread next time, but we rely on the memory fences from
  // mQueueMonitor for thread safety of non-threadsafe tasks.
  nsresult rv = mQueue->mPool->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    // Failed to dispatch, shutdown!
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    mQueue->mIsRunning = false;
    mQueue->mIsShutdown = true;
    mon.NotifyAll();
  }

  return NS_OK;
}

#ifdef DEBUG
void
TaskDispatcher::AssertIsTailDispatcherIfRequired()
{
  MediaTaskQueue* currentQueue = MediaTaskQueue::GetCurrentQueue();
  MOZ_ASSERT_IF(currentQueue && currentQueue->RequiresTailDispatch(),
                this == &currentQueue->TailDispatcher());
}
#endif

} // namespace mozilla
