/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerService.h"

#include "mozilla/dom/PRemoteWorkerParent.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/Services.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsXPCOMPrivate.h"
#include "RemoteWorkerController.h"
#include "RemoteWorkerServiceChild.h"
#include "RemoteWorkerServiceParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

StaticMutex sRemoteWorkerServiceMutex;
StaticRefPtr<RemoteWorkerService> sRemoteWorkerService;

}  // namespace

/* static */
void RemoteWorkerService::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  StaticMutexAutoLock lock(sRemoteWorkerServiceMutex);
  MOZ_ASSERT(!sRemoteWorkerService);

  RefPtr<RemoteWorkerService> service = new RemoteWorkerService();

  if (!XRE_IsParentProcess()) {
    nsresult rv = service->InitializeOnMainThread();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    sRemoteWorkerService = service;
    return;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  nsresult rv = obs->AddObserver(service, "profile-after-change", false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  sRemoteWorkerService = service;
}

/* static */
nsIThread* RemoteWorkerService::Thread() {
  StaticMutexAutoLock lock(sRemoteWorkerServiceMutex);
  MOZ_ASSERT(sRemoteWorkerService);
  MOZ_ASSERT(sRemoteWorkerService->mThread);
  return sRemoteWorkerService->mThread;
}

nsresult RemoteWorkerService::InitializeOnMainThread() {
  // I would like to call this thread "DOM Remote Worker Launcher", but the max
  // length is 16 chars.
  nsresult rv = NS_NewNamedThread("Worker Launcher", getter_AddRefs(mThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_FAILURE;
  }

  rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<RemoteWorkerService> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "InitializeThread", [self]() { self->InitializeOnTargetThread(); });

  rv = mThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

RemoteWorkerService::RemoteWorkerService() { MOZ_ASSERT(NS_IsMainThread()); }

RemoteWorkerService::~RemoteWorkerService() = default;

void RemoteWorkerService::InitializeOnTargetThread() {
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    return;
  }

  RemoteWorkerServiceChild* actor = static_cast<RemoteWorkerServiceChild*>(
      actorChild->SendPRemoteWorkerServiceConstructor());
  if (NS_WARN_IF(!actor)) {
    return;
  }

  // Now we are ready!
  mActor = actor;
}

void RemoteWorkerService::ShutdownOnTargetThread() {
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  MOZ_ASSERT(mActor);

  // Here we need to shutdown the IPC protocol.
  mActor->Send__delete__(mActor);
  mActor = nullptr;

  // Then we can terminate the thread on the main-thread.
  RefPtr<RemoteWorkerService> self = this;
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("ShutdownOnMainThread", [self]() {
        self->mThread->Shutdown();
        self->mThread = nullptr;
      });

  SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
}

NS_IMETHODIMP
RemoteWorkerService::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    MOZ_ASSERT(mThread);

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }

    RefPtr<RemoteWorkerService> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "ShutdownThread", [self]() { self->ShutdownOnTargetThread(); });

    mThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);

    StaticMutexAutoLock lock(sRemoteWorkerServiceMutex);
    sRemoteWorkerService = nullptr;
    return NS_OK;
  }

  MOZ_ASSERT(!strcmp(aTopic, "profile-after-change"));
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "profile-after-change");
  }

  return InitializeOnMainThread();
}

NS_IMPL_ISUPPORTS(RemoteWorkerService, nsIObserver)

}  // namespace dom
}  // namespace mozilla
