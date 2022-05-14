/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteLazyInputStreamThread.h"

#include "ErrorList.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsXPCOMPrivate.h"

using namespace mozilla::ipc;

namespace mozilla {

namespace {

StaticMutex gRemoteLazyThreadMutex;
StaticRefPtr<RemoteLazyInputStreamThread> gRemoteLazyThread;
bool gShutdownHasStarted = false;

class ThreadInitializeRunnable final : public Runnable {
 public:
  ThreadInitializeRunnable() : Runnable("dom::ThreadInitializeRunnable") {}

  NS_IMETHOD
  Run() override {
    StaticMutexAutoLock lock(gRemoteLazyThreadMutex);
    MOZ_ASSERT(gRemoteLazyThread);
    gRemoteLazyThread->InitializeOnMainThread();
    return NS_OK;
  }
};

}  // namespace

NS_IMPL_ISUPPORTS(RemoteLazyInputStreamThread, nsIObserver, nsIEventTarget)

/* static */
RemoteLazyInputStreamThread* RemoteLazyInputStreamThread::Get() {
  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  if (gShutdownHasStarted) {
    return nullptr;
  }

  return gRemoteLazyThread;
}

/* static */
RemoteLazyInputStreamThread* RemoteLazyInputStreamThread::GetOrCreate() {
  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  if (gShutdownHasStarted) {
    return nullptr;
  }

  if (!gRemoteLazyThread) {
    gRemoteLazyThread = new RemoteLazyInputStreamThread();
    if (!gRemoteLazyThread->Initialize()) {
      return nullptr;
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
    SchedulerGroup::Dispatch(TaskCategory::Other, runnable.forget());
    return true;
  }

  InitializeOnMainThread();
  return true;
}

void RemoteLazyInputStreamThread::InitializeOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  nsresult rv =
      obs->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
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

  gShutdownHasStarted = true;
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
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  if (gShutdownHasStarted) {
    return NS_ERROR_NOT_INITIALIZED;
  }

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

bool IsOnDOMFileThread() {
  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  MOZ_ASSERT(!gShutdownHasStarted);
  MOZ_ASSERT(gRemoteLazyThread);

  return gRemoteLazyThread->IsOnCurrentThreadInfallible();
}

void AssertIsOnDOMFileThread() { MOZ_ASSERT(IsOnDOMFileThread()); }

}  // namespace mozilla
