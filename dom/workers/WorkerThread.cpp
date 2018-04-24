/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerThread.h"

#include "mozilla/Assertions.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "EventQueue.h"
#include "mozilla/ThreadEventQueue.h"
#include "mozilla/PerformanceCounter.h"
#include "nsIThreadInternal.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

#ifdef DEBUG
#include "nsThreadManager.h"
#endif

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

// The C stack size. We use the same stack size on all platforms for
// consistency.
const uint32_t kWorkerStackSize = 256 * sizeof(size_t) * 1024;

} // namespace

WorkerThreadFriendKey::WorkerThreadFriendKey()
{
  MOZ_COUNT_CTOR(WorkerThreadFriendKey);
}

WorkerThreadFriendKey::~WorkerThreadFriendKey()
{
  MOZ_COUNT_DTOR(WorkerThreadFriendKey);
}

class WorkerThread::Observer final
  : public nsIThreadObserver
{
  WorkerPrivate* mWorkerPrivate;

public:
  explicit Observer(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  ~Observer()
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_DECL_NSITHREADOBSERVER
};

WorkerThread::WorkerThread()
  : nsThread(MakeNotNull<ThreadEventQueue<mozilla::EventQueue>*>(
               MakeUnique<mozilla::EventQueue>()),
             nsThread::NOT_MAIN_THREAD,
             kWorkerStackSize)
  , mLock("WorkerThread::mLock")
  , mWorkerPrivateCondVar(mLock, "WorkerThread::mWorkerPrivateCondVar")
  , mWorkerPrivate(nullptr)
  , mOtherThreadsDispatchingViaEventTarget(0)
#ifdef DEBUG
  , mAcceptingNonWorkerRunnables(true)
#endif
{
}

WorkerThread::~WorkerThread()
{
  MOZ_ASSERT(!mWorkerPrivate);
  MOZ_ASSERT(!mOtherThreadsDispatchingViaEventTarget);
  MOZ_ASSERT(mAcceptingNonWorkerRunnables);
}

// static
already_AddRefed<WorkerThread>
WorkerThread::Create(const WorkerThreadFriendKey& /* aKey */)
{
  RefPtr<WorkerThread> thread = new WorkerThread();
  if (NS_FAILED(thread->Init(NS_LITERAL_CSTRING("DOM Worker")))) {
    NS_WARNING("Failed to create new thread!");
    return nullptr;
  }

  return thread.forget();
}

void
WorkerThread::SetWorker(const WorkerThreadFriendKey& /* aKey */,
                        WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(PR_GetCurrentThread() == mThread);

  if (aWorkerPrivate) {
    {
      MutexAutoLock lock(mLock);

      MOZ_ASSERT(!mWorkerPrivate);
      MOZ_ASSERT(mAcceptingNonWorkerRunnables);

      mWorkerPrivate = aWorkerPrivate;
#ifdef DEBUG
      mAcceptingNonWorkerRunnables = false;
#endif
    }

    mObserver = new Observer(aWorkerPrivate);
    MOZ_ALWAYS_SUCCEEDS(AddObserver(mObserver));
  } else {
    MOZ_ALWAYS_SUCCEEDS(RemoveObserver(mObserver));
    mObserver = nullptr;

    {
      MutexAutoLock lock(mLock);

      MOZ_ASSERT(mWorkerPrivate);
      MOZ_ASSERT(!mAcceptingNonWorkerRunnables);
      MOZ_ASSERT(!mOtherThreadsDispatchingViaEventTarget,
                 "XPCOM Dispatch hapenning at the same time our thread is "
                 "being unset! This should not be possible!");

      while (mOtherThreadsDispatchingViaEventTarget) {
        mWorkerPrivateCondVar.Wait();
      }

#ifdef DEBUG
      mAcceptingNonWorkerRunnables = true;
#endif
      mWorkerPrivate = nullptr;
    }
  }
}

nsresult
WorkerThread::DispatchPrimaryRunnable(const WorkerThreadFriendKey& /* aKey */,
                                      already_AddRefed<nsIRunnable> aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

#ifdef DEBUG
  MOZ_ASSERT(PR_GetCurrentThread() != mThread);
  MOZ_ASSERT(runnable);
  {
    MutexAutoLock lock(mLock);

    MOZ_ASSERT(!mWorkerPrivate);
    MOZ_ASSERT(mAcceptingNonWorkerRunnables);
  }
#endif

  nsresult rv = nsThread::Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
WorkerThread::DispatchAnyThread(const WorkerThreadFriendKey& /* aKey */,
                       already_AddRefed<WorkerRunnable> aWorkerRunnable)
{
  // May be called on any thread!

#ifdef DEBUG
  {
    const bool onWorkerThread = PR_GetCurrentThread() == mThread;
    {
      MutexAutoLock lock(mLock);

      MOZ_ASSERT(mWorkerPrivate);
      MOZ_ASSERT(!mAcceptingNonWorkerRunnables);

      if (onWorkerThread) {
        mWorkerPrivate->AssertIsOnWorkerThread();
      }
    }
  }
#endif
  nsCOMPtr<nsIRunnable> runnable(aWorkerRunnable);

  nsresult rv = nsThread::Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We don't need to notify the worker's condition variable here because we're
  // being called from worker-controlled code and it will make sure to wake up
  // the worker thread if needed.

  return NS_OK;
}

NS_IMETHODIMP
WorkerThread::DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  return Dispatch(runnable.forget(), aFlags);
}

NS_IMETHODIMP
WorkerThread::Dispatch(already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags)
{
  // May be called on any thread!
  nsCOMPtr<nsIRunnable> runnable(aRunnable); // in case we exit early

  // Workers only support asynchronous dispatch.
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }

  const bool onWorkerThread = PR_GetCurrentThread() == mThread;

  if (GetSchedulerLoggingEnabled() && onWorkerThread && mWorkerPrivate) {
    PerformanceCounter* performanceCounter = mWorkerPrivate->GetPerformanceCounter();
    if (performanceCounter) {
      performanceCounter->IncrementDispatchCounter(DispatchCategory::Worker);
    }
  }

#ifdef DEBUG
  if (runnable && !onWorkerThread) {
    nsCOMPtr<nsICancelableRunnable> cancelable = do_QueryInterface(runnable);

    {
      MutexAutoLock lock(mLock);

      // Only enforce cancelable runnables after we've started the worker loop.
      if (!mAcceptingNonWorkerRunnables) {
        MOZ_ASSERT(cancelable,
                   "Only nsICancelableRunnable may be dispatched to a worker!");
      }
    }
  }
#endif

  WorkerPrivate* workerPrivate = nullptr;
  if (onWorkerThread) {
    // No need to lock here because it is only modified on this thread.
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();

    workerPrivate = mWorkerPrivate;
  } else {
    MutexAutoLock lock(mLock);

    MOZ_ASSERT(mOtherThreadsDispatchingViaEventTarget < UINT32_MAX);

    if (mWorkerPrivate) {
      workerPrivate = mWorkerPrivate;

      // Incrementing this counter will make the worker thread sleep if it
      // somehow tries to unset mWorkerPrivate while we're using it.
      mOtherThreadsDispatchingViaEventTarget++;
    }
  }

  nsresult rv;
  if (runnable && onWorkerThread) {
    RefPtr<WorkerRunnable> workerRunnable = workerPrivate->MaybeWrapAsWorkerRunnable(runnable.forget());
    rv = nsThread::Dispatch(workerRunnable.forget(), NS_DISPATCH_NORMAL);
  } else {
    rv = nsThread::Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  }

  if (!onWorkerThread && workerPrivate) {
    // We need to wake the worker thread if we're not already on the right
    // thread and the dispatch succeeded.
    if (NS_SUCCEEDED(rv)) {
      MutexAutoLock workerLock(workerPrivate->mMutex);

      workerPrivate->mCondVar.Notify();
    }

    // Now unset our waiting flag.
    {
      MutexAutoLock lock(mLock);

      MOZ_ASSERT(mOtherThreadsDispatchingViaEventTarget);

      if (!--mOtherThreadsDispatchingViaEventTarget) {
        mWorkerPrivateCondVar.Notify();
      }
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
WorkerThread::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t
WorkerThread::RecursionDepth(const WorkerThreadFriendKey& /* aKey */) const
{
  MOZ_ASSERT(PR_GetCurrentThread() == mThread);

  return mNestedEventLoopDepth;
}

PerformanceCounter*
WorkerThread::GetPerformanceCounter(nsIRunnable* aEvent)
{
  if (mWorkerPrivate) {
    return mWorkerPrivate->GetPerformanceCounter();
  }
  return nullptr;
}

NS_IMPL_ISUPPORTS(WorkerThread::Observer, nsIThreadObserver)

NS_IMETHODIMP
WorkerThread::Observer::OnDispatchedEvent()
{
  MOZ_CRASH("OnDispatchedEvent() should never be called!");
}

NS_IMETHODIMP
WorkerThread::Observer::OnProcessNextEvent(nsIThreadInternal* /* aThread */,
                                           bool aMayWait)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  // If the PBackground child is not created yet, then we must permit
  // blocking event processing to support
  // BackgroundChild::GetOrCreateCreateForCurrentThread(). If this occurs
  // then we are spinning on the event queue at the start of
  // PrimaryWorkerRunnable::Run() and don't want to process the event in
  // mWorkerPrivate yet.
  if (aMayWait) {
    MOZ_ASSERT(CycleCollectedJSContext::Get()->RecursionDepth() == 2);
    MOZ_ASSERT(!BackgroundChild::GetForCurrentThread());
    return NS_OK;
  }

  mWorkerPrivate->OnProcessNextEvent();
  return NS_OK;
}

NS_IMETHODIMP
WorkerThread::Observer::AfterProcessNextEvent(nsIThreadInternal* /* aThread */,
                                              bool /* aEventWasProcessed */)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  mWorkerPrivate->AfterProcessNextEvent();
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
