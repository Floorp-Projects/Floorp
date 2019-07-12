/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PServiceWorkerContainerChild.h"
#include "mozilla/dom/WorkerRef.h"

#include "RemoteServiceWorkerContainerImpl.h"

namespace mozilla {
namespace dom {

void ServiceWorkerContainerChild::ActorDestroy(ActorDestroyReason aReason) {
  mIPCWorkerRef = nullptr;

  if (mOwner) {
    mOwner->RevokeActor(this);
    MOZ_DIAGNOSTIC_ASSERT(!mOwner);
  }
}

// static
ServiceWorkerContainerChild* ServiceWorkerContainerChild::Create() {
  ServiceWorkerContainerChild* actor = new ServiceWorkerContainerChild();

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_DIAGNOSTIC_ASSERT(workerPrivate);

    RefPtr<IPCWorkerRefHelper<ServiceWorkerContainerChild>> helper =
        new IPCWorkerRefHelper<ServiceWorkerContainerChild>(actor);

    actor->mIPCWorkerRef = IPCWorkerRef::Create(
        workerPrivate, "ServiceWorkerContainerChild",
        [helper] { helper->Actor()->MaybeStartTeardown(); });
    if (NS_WARN_IF(!actor->mIPCWorkerRef)) {
      delete actor;
      return nullptr;
    }
  }

  return actor;
}

ServiceWorkerContainerChild::ServiceWorkerContainerChild()
    : mOwner(nullptr), mTeardownStarted(false) {}

void ServiceWorkerContainerChild::SetOwner(
    RemoteServiceWorkerContainerImpl* aOwner) {
  MOZ_DIAGNOSTIC_ASSERT(!mOwner);
  MOZ_DIAGNOSTIC_ASSERT(aOwner);
  mOwner = aOwner;
}

void ServiceWorkerContainerChild::RevokeOwner(
    RemoteServiceWorkerContainerImpl* aOwner) {
  MOZ_DIAGNOSTIC_ASSERT(mOwner);
  MOZ_DIAGNOSTIC_ASSERT(aOwner == mOwner);
  mOwner = nullptr;
}

void ServiceWorkerContainerChild::MaybeStartTeardown() {
  if (mTeardownStarted) {
    return;
  }
  mTeardownStarted = true;
}

}  // namespace dom
}  // namespace mozilla
