/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerThread.h"

#include "mozilla/Assertions.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "nsIThreadInternal.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

#ifdef DEBUG
#include "nsThreadManager.h"
#endif

namespace mozilla {
namespace dom {
namespace workers {

using namespace mozilla::ipc;

namespace {

// The C stack size. We use the same stack size on all platforms for
// consistency.
const uint32_t kWorkerStackSize = 256 * sizeof(size_t) * 1024;

} // anonymous namespace

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
  : nsThread(nsThread::NOT_MAIN_THREAD, kWorkerStackSize)
  , mWorkerPrivateCondVar(mLock, "WorkerThread::mWorkerPrivateCondVar")
  , mWorkerPrivate(nullptr)
  , mOtherThreadsDispatchingViaEventTarget(0)
  , mAcceptingNonWorkerRunnables(true)
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
  MOZ_ASSERT(nsThreadManager::get());

  nsRefPtr<WorkerThread> thread = new WorkerThread();
  if (NS_FAILED(thread->Init())) {
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
      mAcceptingNonWorkerRunnables = false;
    }

    mObserver = new Observer(aWorkerPrivate);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(AddObserver(mObserver)));
  } else {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(RemoveObserver(mObserver)));
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

      mAcceptingNonWorkerRunnables = true;
      mWorkerPrivate = nullptr;
    }
  }
}

nsresult
WorkerThread::DispatchPrimaryRunnable(const WorkerThreadFriendKey& /* aKey */,
                                      nsIRunnable* aRunnable)
{
#ifdef DEBUG
  MOZ_ASSERT(PR_GetCurrentThread() != mThread);
  MOZ_ASSERT(aRunnable);
  {
    MutexAutoLock lock(mLock);

    MOZ_ASSERT(!mWorkerPrivate);
    MOZ_ASSERT(mAcceptingNonWorkerRunnables);
  }
#endif

  nsresult rv = nsThread::Dispatch(aRunnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
WorkerThread::Dispatch(const WorkerThreadFriendKey& /* aKey */,
                       WorkerRunnable* aWorkerRunnable)
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

  nsresult rv = nsThread::Dispatch(aWorkerRunnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We don't need to notify the worker's condition variable here because we're
  // being called from worker-controlled code and it will make sure to wake up
  // the worker thread if needed.

  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED0(WorkerThread, nsThread)

NS_IMETHODIMP
WorkerThread::Dispatch(nsIRunnable* aRunnable, uint32_t aFlags)
{
  // May be called on any thread!

  // Workers only support asynchronous dispatch.
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }

  const bool onWorkerThread = PR_GetCurrentThread() == mThread;

#ifdef DEBUG
  if (aRunnable && !onWorkerThread) {
    nsCOMPtr<nsICancelableRunnable> cancelable = do_QueryInterface(aRunnable);

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

  nsIRunnable* runnableToDispatch;
  nsRefPtr<WorkerRunnable> workerRunnable;

  if (aRunnable && onWorkerThread) {
    workerRunnable = workerPrivate->MaybeWrapAsWorkerRunnable(aRunnable);
    runnableToDispatch = workerRunnable;
  } else {
    runnableToDispatch = aRunnable;
  }

  nsresult rv = nsThread::Dispatch(runnableToDispatch, NS_DISPATCH_NORMAL);

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

uint32_t
WorkerThread::RecursionDepth(const WorkerThreadFriendKey& /* aKey */) const
{
  MOZ_ASSERT(PR_GetCurrentThread() == mThread);

  return mNestedEventLoopDepth;
}

NS_IMPL_ISUPPORTS(WorkerThread::Observer, nsIThreadObserver)

NS_IMETHODIMP
WorkerThread::Observer::OnDispatchedEvent(nsIThreadInternal* /* aThread */)
{
  MOZ_CRASH("OnDispatchedEvent() should never be called!");
}

NS_IMETHODIMP
WorkerThread::Observer::OnProcessNextEvent(nsIThreadInternal* /* aThread */,
                                           bool aMayWait,
                                           uint32_t aRecursionDepth)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  // If the PBackground child is not created yet, then we must permit
  // blocking event processing to support SynchronouslyCreatePBackground().
  // If this occurs then we are spinning on the event queue at the start of
  // PrimaryWorkerRunnable::Run() and don't want to process the event in
  // mWorkerPrivate yet.
  if (aMayWait) {
    MOZ_ASSERT(aRecursionDepth == 2);
    MOZ_ASSERT(!BackgroundChild::GetForCurrentThread());
    return NS_OK;
  }

  mWorkerPrivate->OnProcessNextEvent(aRecursionDepth);
  return NS_OK;
}

NS_IMETHODIMP
WorkerThread::Observer::AfterProcessNextEvent(nsIThreadInternal* /* aThread */,
                                              uint32_t aRecursionDepth,
                                              bool /* aEventWasProcessed */)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  mWorkerPrivate->AfterProcessNextEvent(aRecursionDepth);
  return NS_OK;
}

} // namespace workers
} // namespace dom
} // namespace mozilla
