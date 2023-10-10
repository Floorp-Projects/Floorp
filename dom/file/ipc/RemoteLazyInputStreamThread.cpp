/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteLazyInputStreamThread.h"

#include "ErrorList.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsXPCOMPrivate.h"

using namespace mozilla::ipc;

namespace mozilla {

namespace {

StaticMutex gRemoteLazyThreadMutex;
StaticRefPtr<RemoteLazyInputStreamThread> gRemoteLazyThread;

class ThreadInitializeRunnable final : public Runnable {
 public:
  ThreadInitializeRunnable() : Runnable("dom::ThreadInitializeRunnable") {}

  NS_IMETHOD
  Run() override {
    StaticMutexAutoLock lock(gRemoteLazyThreadMutex);
    MOZ_ASSERT(gRemoteLazyThread);
    if (NS_WARN_IF(!gRemoteLazyThread->InitializeOnMainThread())) {
      // RemoteLazyInputStreamThread::GetOrCreate might have handed out a
      // pointer to our thread already at this point such that we cannot
      // just do gRemoteLazyThread = nullptr; here.
      MOZ_DIAGNOSTIC_ASSERT(
          false, "Async gRemoteLazyThread->InitializeOnMainThread() failed.");
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }
};

}  // namespace

NS_IMPL_ISUPPORTS(RemoteLazyInputStreamThread, nsIObserver, nsIEventTarget,
                  nsISerialEventTarget, nsIDirectTaskDispatcher)

bool RLISThreadIsInOrBeyondShutdown() {
  // ShutdownPhase::XPCOMShutdownThreads matches
  // obs->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
  return AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads);
}

/* static */
RemoteLazyInputStreamThread* RemoteLazyInputStreamThread::Get() {
  if (RLISThreadIsInOrBeyondShutdown()) {
    return nullptr;
  }

  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  return gRemoteLazyThread;
}

/* static */
RemoteLazyInputStreamThread* RemoteLazyInputStreamThread::GetOrCreate() {
  if (RLISThreadIsInOrBeyondShutdown()) {
    return nullptr;
  }

  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  if (!gRemoteLazyThread) {
    gRemoteLazyThread = new RemoteLazyInputStreamThread();
    if (!gRemoteLazyThread->Initialize()) {
      gRemoteLazyThread = nullptr;
    }
  }

  return gRemoteLazyThread;
}

bool RemoteLazyInputStreamThread::Initialize() {
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("RemoteLzyStream", getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  mThread = thread;

  if (!NS_IsMainThread()) {
    RefPtr<Runnable> runnable = new ThreadInitializeRunnable();
    nsresult rv = SchedulerGroup::Dispatch(runnable.forget());
    return !NS_WARN_IF(NS_FAILED(rv));
  }

  return InitializeOnMainThread();
}

bool RemoteLazyInputStreamThread::InitializeOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return false;
  }

  nsresult rv =
      obs->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
  return !NS_WARN_IF(NS_FAILED(rv));
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::Observe(nsISupports* aSubject, const char* aTopic,
                                     const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID));

  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }

  gRemoteLazyThread = nullptr;

  return NS_OK;
}

// nsIEventTarget

NS_IMETHODIMP_(bool)
RemoteLazyInputStreamThread::IsOnCurrentThreadInfallible() {
  return mThread->IsOnCurrentThread();
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::IsOnCurrentThread(bool* aRetval) {
  return mThread->IsOnCurrentThread(aRetval);
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                                      uint32_t aFlags) {
  if (RLISThreadIsInOrBeyondShutdown()) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  return mThread->Dispatch(runnable.forget(), aFlags);
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::DispatchFromScript(nsIRunnable* aRunnable,
                                                uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  return Dispatch(runnable.forget(), aFlags);
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::DelayedDispatch(already_AddRefed<nsIRunnable>,
                                             uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::RegisterShutdownTask(nsITargetShutdownTask*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::UnregisterShutdownTask(nsITargetShutdownTask*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::DispatchDirectTask(
    already_AddRefed<nsIRunnable> aRunnable) {
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  nsCOMPtr<nsIDirectTaskDispatcher> dispatcher = do_QueryInterface(mThread);

  if (dispatcher) {
    return dispatcher->DispatchDirectTask(runnable.forget());
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP RemoteLazyInputStreamThread::DrainDirectTasks() {
  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  nsCOMPtr<nsIDirectTaskDispatcher> dispatcher = do_QueryInterface(mThread);

  if (dispatcher) {
    return dispatcher->DrainDirectTasks();
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP RemoteLazyInputStreamThread::HaveDirectTasks(bool* aValue) {
  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  nsCOMPtr<nsIDirectTaskDispatcher> dispatcher = do_QueryInterface(mThread);

  if (dispatcher) {
    return dispatcher->HaveDirectTasks(aValue);
  }

  return NS_ERROR_FAILURE;
}

bool IsOnDOMFileThread() {
  MOZ_ASSERT(!RLISThreadIsInOrBeyondShutdown());

  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);
  MOZ_ASSERT(gRemoteLazyThread);

  return gRemoteLazyThread->IsOnCurrentThreadInfallible();
}

void AssertIsOnDOMFileThread() { MOZ_ASSERT(IsOnDOMFileThread()); }

}  // namespace mozilla
