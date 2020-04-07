/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStreamThread.h"

#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsXPCOMPrivate.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

StaticMutex gIPCBlobThreadMutex;
StaticRefPtr<IPCBlobInputStreamThread> gIPCBlobThread;
bool gShutdownHasStarted = false;

class ThreadInitializeRunnable final : public Runnable {
 public:
  ThreadInitializeRunnable() : Runnable("dom::ThreadInitializeRunnable") {}

  NS_IMETHOD
  Run() override {
    mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);
    MOZ_ASSERT(gIPCBlobThread);
    gIPCBlobThread->InitializeOnMainThread();
    return NS_OK;
  }
};

class MigrateActorRunnable final : public Runnable {
 public:
  explicit MigrateActorRunnable(IPCBlobInputStreamChild* aActor)
      : Runnable("dom::MigrateActorRunnable"), mActor(aActor) {
    MOZ_ASSERT(mActor);
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(mActor->State() == IPCBlobInputStreamChild::eInactiveMigrating);

    PBackgroundChild* actorChild =
        BackgroundChild::GetOrCreateForCurrentThread();
    if (!actorChild) {
      return NS_OK;
    }

    if (actorChild->SendPIPCBlobInputStreamConstructor(mActor, mActor->ID(),
                                                       mActor->Size())) {
      mActor->Migrated();
    }

    return NS_OK;
  }

 private:
  ~MigrateActorRunnable() = default;

  RefPtr<IPCBlobInputStreamChild> mActor;
};

}  // namespace

NS_IMPL_ISUPPORTS(IPCBlobInputStreamThread, nsIObserver, nsIEventTarget)

/* static */
bool IPCBlobInputStreamThread::IsOnFileEventTarget(
    nsIEventTarget* aEventTarget) {
  MOZ_ASSERT(aEventTarget);

  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);
  return gIPCBlobThread && aEventTarget == gIPCBlobThread->mThread;
}

/* static */
IPCBlobInputStreamThread* IPCBlobInputStreamThread::Get() {
  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);

  if (gShutdownHasStarted) {
    return nullptr;
  }

  return gIPCBlobThread;
}

/* static */
IPCBlobInputStreamThread* IPCBlobInputStreamThread::GetOrCreate() {
  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);

  if (gShutdownHasStarted) {
    return nullptr;
  }

  if (!gIPCBlobThread) {
    gIPCBlobThread = new IPCBlobInputStreamThread();
    if (!gIPCBlobThread->Initialize()) {
      return nullptr;
    }
  }

  return gIPCBlobThread;
}

bool IPCBlobInputStreamThread::Initialize() {
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("DOM File", getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  mThread = thread;

  if (!mPendingActors.IsEmpty()) {
    for (uint32_t i = 0; i < mPendingActors.Length(); ++i) {
      MigrateActorInternal(mPendingActors[i]);
    }

    mPendingActors.Clear();
  }

  if (!NS_IsMainThread()) {
    RefPtr<Runnable> runnable = new ThreadInitializeRunnable();
    SchedulerGroup::Dispatch(TaskCategory::Other, runnable.forget());
    return true;
  }

  InitializeOnMainThread();
  return true;
}

void IPCBlobInputStreamThread::InitializeOnMainThread() {
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
IPCBlobInputStreamThread::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID));

  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }

  gShutdownHasStarted = true;
  gIPCBlobThread = nullptr;

  return NS_OK;
}

void IPCBlobInputStreamThread::MigrateActor(IPCBlobInputStreamChild* aActor) {
  MOZ_ASSERT(aActor->State() == IPCBlobInputStreamChild::eInactiveMigrating);

  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);

  if (gShutdownHasStarted) {
    return;
  }

  if (!mThread) {
    // The thread is not initialized yet.
    mPendingActors.AppendElement(aActor);
    return;
  }

  MigrateActorInternal(aActor);
}

void IPCBlobInputStreamThread::MigrateActorInternal(
    IPCBlobInputStreamChild* aActor) {
  RefPtr<Runnable> runnable = new MigrateActorRunnable(aActor);
  mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

// nsIEventTarget

NS_IMETHODIMP_(bool)
IPCBlobInputStreamThread::IsOnCurrentThreadInfallible() {
  return mThread->IsOnCurrentThread();
}

NS_IMETHODIMP
IPCBlobInputStreamThread::IsOnCurrentThread(bool* aRetval) {
  return mThread->IsOnCurrentThread(aRetval);
}

NS_IMETHODIMP
IPCBlobInputStreamThread::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                                   uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);

  if (gShutdownHasStarted) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mThread->Dispatch(runnable.forget(), aFlags);
}

NS_IMETHODIMP
IPCBlobInputStreamThread::DispatchFromScript(nsIRunnable* aRunnable,
                                             uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  return Dispatch(runnable.forget(), aFlags);
}

NS_IMETHODIMP
IPCBlobInputStreamThread::DelayedDispatch(already_AddRefed<nsIRunnable>,
                                          uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

bool IsOnDOMFileThread() {
  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);

  MOZ_ASSERT(!gShutdownHasStarted);
  MOZ_ASSERT(gIPCBlobThread);

  return gIPCBlobThread->IsOnCurrentThreadInfallible();
}

void AssertIsOnDOMFileThread() { MOZ_ASSERT(IsOnDOMFileThread()); }

}  // namespace dom
}  // namespace mozilla
