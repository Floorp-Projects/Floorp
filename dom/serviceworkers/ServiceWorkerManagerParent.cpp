/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManagerParent.h"
#include "ServiceWorkerUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"

namespace mozilla {

using namespace ipc;

namespace dom {

ServiceWorkerManagerParent::ServiceWorkerManagerParent() {
  AssertIsOnBackgroundThread();
}

ServiceWorkerManagerParent::~ServiceWorkerManagerParent() {
  AssertIsOnBackgroundThread();
}

mozilla::ipc::IPCResult ServiceWorkerManagerParent::RecvRegister(
    const ServiceWorkerRegistrationData& aData) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!BackgroundParent::IsOtherProcessActor(Manager()));

  // Basic validation.
  if (aData.scope().IsEmpty() ||
      aData.principal().type() == PrincipalInfo::TNullPrincipalInfo ||
      aData.principal().type() == PrincipalInfo::TSystemPrincipalInfo) {
    return IPC_FAIL_NO_REASON(this);
  }

  // If false then we have shutdown during the process of trying to update the
  // registrar. We can give up on this modification.
  if (const RefPtr<dom::ServiceWorkerRegistrar> service =
          dom::ServiceWorkerRegistrar::Get()) {
    service->RegisterServiceWorker(aData);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ServiceWorkerManagerParent::RecvUnregister(
    const PrincipalInfo& aPrincipalInfo, const nsString& aScope) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!BackgroundParent::IsOtherProcessActor(Manager()));

  // Basic validation.
  if (aScope.IsEmpty() ||
      aPrincipalInfo.type() == PrincipalInfo::TNullPrincipalInfo ||
      aPrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    return IPC_FAIL_NO_REASON(this);
  }

  // If false then we have shutdown during the process of trying to update the
  // registrar. We can give up on this modification.
  if (const RefPtr<dom::ServiceWorkerRegistrar> service =
          dom::ServiceWorkerRegistrar::Get()) {
    service->UnregisterServiceWorker(aPrincipalInfo,
                                     NS_ConvertUTF16toUTF8(aScope));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ServiceWorkerManagerParent::RecvPropagateUnregister(
    const PrincipalInfo& aPrincipalInfo, const nsString& aScope) {
  AssertIsOnBackgroundThread();

  RefPtr<dom::ServiceWorkerRegistrar> service =
      dom::ServiceWorkerRegistrar::Get();
  MOZ_ASSERT(service);

  // It's possible that we don't have any ServiceWorkerManager managing this
  // scope but we still need to unregister it from the ServiceWorkerRegistrar.
  service->UnregisterServiceWorker(aPrincipalInfo,
                                   NS_ConvertUTF16toUTF8(aScope));

  // There is no longer any point to propagating because the only sender is the
  // one and only ServiceWorkerManager, but it is necessary for us to have run
  // the unregister call above because until Bug 1183245 is fixed,
  // nsIServiceWorkerManager.propagateUnregister() is a de facto API for
  // clearing ServiceWorker registrations by Sanitizer.jsm via
  // ServiceWorkerCleanUp.jsm, as well as devtools "unregister" affordance and
  // the no-longer-relevant about:serviceworkers UI.

  return IPC_OK();
}

void ServiceWorkerManagerParent::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
}

}  // namespace dom
}  // namespace mozilla
