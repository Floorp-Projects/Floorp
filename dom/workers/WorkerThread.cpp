/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerThread.h"

#include <utility>
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/EventQueue.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/NotNull.h"
#include "mozilla/ThreadEventQueue.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsICancelableRunnable.h"
#include "nsIEventTarget.h"
#include "nsIRunnable.h"
#include "nsIThreadInternal.h"
#include "nsString.h"
#include "prthread.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

// The C stack size. We use the same stack size on all platforms for
// consistency.
//
// Note: Our typical equation of 256 machine words works out to 2MB on 64-bit
// platforms. Since that works out to the size of a VM huge page, that can
// sometimes lead to an OS allocating an entire huge page for the stack at once.
// To avoid this, we subtract the size of 2 pages, to be safe.
const uint32_t kWorkerStackSize = 256 * sizeof(size_t) * 1024 - 8192;

}  // namespace

WorkerThreadFriendKey::WorkerThreadFriendKey() {
  MOZ_COUNT_CTOR(WorkerThreadFriendKey);
}

WorkerThreadFriendKey::~WorkerThreadFriendKey() {
  MOZ_COUNT_DTOR(WorkerThreadFriendKey);
}

class WorkerThread::Observer final : public nsIThreadObserver {
  WorkerPrivate* mWorkerPrivate;

 public:
  explicit Observer(WorkerPrivate* aWorkerPrivate)
      : mWorkerPrivate(aWorkerPrivate) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_DECL_THREADSAFE_ISUPPORTS

 private:
  ~Observer() { mWorkerPrivate->AssertIsOnWorkerThread(); }

  NS_DECL_NSITHREADOBSERVER
};

WorkerThread::WorkerThread(ConstructorKey)
    : nsThread(
          MakeNotNull<ThreadEventQueue*>(MakeUnique<mozilla::EventQueue>()),
          nsThread::NOT_MAIN_THREAD, {.stackSize = kWorkerStackSize}),
      mLock("WorkerThread::mLock"),
      mWorkerPrivateCondVar(mLock, "WorkerThread::mWorkerPrivateCondVar"),
      mWorkerPrivate(nullptr),
      mOtherThreadsDispatchingViaEventTarget(0)
#ifdef DEBUG
      ,
      mAcceptingNonWorkerRunnables(true)
#endif
{
}

WorkerThread::~WorkerThread() {
  MOZ_ASSERT(!mWorkerPrivate);
  MOZ_ASSERT(!mOtherThreadsDispatchingViaEventTarget);
  MOZ_ASSERT(mAcceptingNonWorkerRunnables);
}

// static
SafeRefPtr<WorkerThread> WorkerThread::Create(
    const WorkerThreadFriendKey& /* aKey */) {
  SafeRefPtr<WorkerThread> thread =
      MakeSafeRefPtr<WorkerThread>(ConstructorKey());
  if (NS_FAILED(thread->Init("DOM Worker"_ns))) {
    NS_WARNING("Failed to create new thread!");
    return nullptr;
  }

  return thread;
}

void WorkerThread::SetWorker(const WorkerThreadFriendKey& /* aKey */,
                             WorkerPrivate* aWorkerPrivate) {
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
      // mOtherThreadsDispatchingViaEventTarget can still be non-zero here
      // because WorkerThread::Dispatch isn't atomic so a thread initiating
      // dispatch can have dispatched a runnable at this thread allowing us to
      // begin shutdown before that thread gets a chance to decrement
      // mOtherThreadsDispatchingViaEventTarget back to 0.  So we need to wait
      // for that.
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

nsresult WorkerThread::DispatchPrimaryRunnable(
    const WorkerThreadFriendKey& /* aKey */,
    already_AddRefed<nsIRunnable> aRunnable) {
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

nsresult WorkerThread::DispatchAnyThread(
    const WorkerThreadFriendKey& /* aKey */,
    already_AddRefed<WorkerRunnable> aWorkerRunnable) {
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
WorkerThread::DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  return Dispatch(runnable.forget(), aFlags);
}

NS_IMETHODIMP
WorkerThread::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                       uint32_t aFlags) {
  // May be called on any thread!
  nsCOMPtr<nsIRunnable> runnable(aRunnable);  // in case we exit early

  // Workers only support asynchronous dispatch.
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }

  const bool onWorkerThread = PR_GetCurrentThread() == mThread;

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
    RefPtr<WorkerRunnable> workerRunnable =
        workerPrivate->MaybeWrapAsWorkerRunnable(runnable.forget());
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
WorkerThread::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t WorkerThread::RecursionDepth(
    const WorkerThreadFriendKey& /* aKey */) const {
  MOZ_ASSERT(PR_GetCurrentThread() == mThread);

  return mNestedEventLoopDepth;
}

NS_IMETHODIMP
WorkerThread::HasPendingEvents(bool* aResult) {
  MOZ_ASSERT(aResult);
  const bool onWorkerThread = PR_GetCurrentThread() == mThread;
  // If is on the worker thread, call nsThread::HasPendingEvents directly.
  if (onWorkerThread) {
    return nsThread::HasPendingEvents(aResult);
  }
  // Checking if is on the parent thread, otherwise, returns unexpected error.
  {
    MutexAutoLock lock(mLock);
    // return directly if the mWorkerPrivate has not yet set or had already
    // unset
    if (!mWorkerPrivate) {
      *aResult = false;
      return NS_OK;
    }
    if (!mWorkerPrivate->IsOnParentThread()) {
      *aResult = false;
      return NS_ERROR_UNEXPECTED;
    }
  }
  *aResult = mEvents->HasPendingEvent();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(WorkerThread::Observer, nsIThreadObserver)

NS_IMETHODIMP
WorkerThread::Observer::OnDispatchedEvent() {
  MOZ_CRASH("OnDispatchedEvent() should never be called!");
}

NS_IMETHODIMP
WorkerThread::Observer::OnProcessNextEvent(nsIThreadInternal* /* aThread */,
                                           bool aMayWait) {
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
                                              bool /* aEventWasProcessed */) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  mWorkerPrivate->AfterProcessNextEvent();
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
