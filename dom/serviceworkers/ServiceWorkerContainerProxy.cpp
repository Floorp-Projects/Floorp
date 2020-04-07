/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerContainerProxy.h"

#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::AssertIsOnBackgroundThread;

ServiceWorkerContainerProxy::~ServiceWorkerContainerProxy() {
  // Any thread
  MOZ_DIAGNOSTIC_ASSERT(!mActor);
}

ServiceWorkerContainerProxy::ServiceWorkerContainerProxy(
    ServiceWorkerContainerParent* aActor)
    : mActor(aActor) {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(mActor);

  // The container does not directly listen for updates, so we don't need
  // to immediately initialize.  The controllerchange event comes via the
  // ClientSource associated with the ServiceWorkerContainer's bound global.
}

void ServiceWorkerContainerProxy::RevokeActor(
    ServiceWorkerContainerParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(mActor);
  MOZ_DIAGNOSTIC_ASSERT(mActor == aActor);
  mActor = nullptr;
}

RefPtr<ServiceWorkerRegistrationPromise> ServiceWorkerContainerProxy::Register(
    const ClientInfo& aClientInfo, const nsCString& aScopeURL,
    const nsCString& aScriptURL, ServiceWorkerUpdateViaCache aUpdateViaCache) {
  AssertIsOnBackgroundThread();

  RefPtr<ServiceWorkerRegistrationPromise::Private> promise =
      new ServiceWorkerRegistrationPromise::Private(__func__);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__,
      [aClientInfo, aScopeURL, aScriptURL, aUpdateViaCache, promise]() mutable {
        auto scopeExit = MakeScopeExit(
            [&] { promise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__); });

        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        NS_ENSURE_TRUE_VOID(swm);

        swm->Register(aClientInfo, aScopeURL, aScriptURL, aUpdateViaCache)
            ->ChainTo(promise.forget(), __func__);

        scopeExit.release();
      });

  MOZ_ALWAYS_SUCCEEDS(
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));

  return promise;
}

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerContainerProxy::GetRegistration(const ClientInfo& aClientInfo,
                                             const nsCString& aURL) {
  AssertIsOnBackgroundThread();

  RefPtr<ServiceWorkerRegistrationPromise::Private> promise =
      new ServiceWorkerRegistrationPromise::Private(__func__);

  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction(__func__, [aClientInfo, aURL, promise]() mutable {
        auto scopeExit = MakeScopeExit(
            [&] { promise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__); });

        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        NS_ENSURE_TRUE_VOID(swm);

        swm->GetRegistration(aClientInfo, aURL)
            ->ChainTo(promise.forget(), __func__);

        scopeExit.release();
      });

  MOZ_ALWAYS_SUCCEEDS(
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));

  return promise;
}

RefPtr<ServiceWorkerRegistrationListPromise>
ServiceWorkerContainerProxy::GetRegistrations(const ClientInfo& aClientInfo) {
  AssertIsOnBackgroundThread();

  RefPtr<ServiceWorkerRegistrationListPromise::Private> promise =
      new ServiceWorkerRegistrationListPromise::Private(__func__);

  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction(__func__, [aClientInfo, promise]() mutable {
        auto scopeExit = MakeScopeExit(
            [&] { promise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__); });

        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        NS_ENSURE_TRUE_VOID(swm);

        swm->GetRegistrations(aClientInfo)->ChainTo(promise.forget(), __func__);

        scopeExit.release();
      });

  MOZ_ALWAYS_SUCCEEDS(
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));

  return promise;
}

RefPtr<ServiceWorkerRegistrationPromise> ServiceWorkerContainerProxy::GetReady(
    const ClientInfo& aClientInfo) {
  AssertIsOnBackgroundThread();

  RefPtr<ServiceWorkerRegistrationPromise::Private> promise =
      new ServiceWorkerRegistrationPromise::Private(__func__);

  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction(__func__, [aClientInfo, promise]() mutable {
        auto scopeExit = MakeScopeExit(
            [&] { promise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__); });

        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        NS_ENSURE_TRUE_VOID(swm);

        swm->WhenReady(aClientInfo)->ChainTo(promise.forget(), __func__);

        scopeExit.release();
      });

  MOZ_ALWAYS_SUCCEEDS(
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));

  return promise;
}

}  // namespace dom
}  // namespace mozilla
