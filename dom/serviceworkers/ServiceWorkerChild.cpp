/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerChild.h"
#include "ServiceWorker.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom {

void ServiceWorkerChild::ActorDestroy(ActorDestroyReason aReason) {
  mIPCWorkerRef = nullptr;

  if (mOwner) {
    mOwner->RevokeActor(this);
    MOZ_DIAGNOSTIC_ASSERT(!mOwner);
  }
}

// static
RefPtr<ServiceWorkerChild> ServiceWorkerChild::Create() {
  RefPtr<ServiceWorkerChild> actor = new ServiceWorkerChild();

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_DIAGNOSTIC_ASSERT(workerPrivate);

    RefPtr<IPCWorkerRefHelper<ServiceWorkerChild>> helper =
        new IPCWorkerRefHelper<ServiceWorkerChild>(actor);

    actor->mIPCWorkerRef = IPCWorkerRef::Create(
        workerPrivate, "ServiceWorkerChild",
        [helper] { helper->Actor()->MaybeStartTeardown(); });

    if (NS_WARN_IF(!actor->mIPCWorkerRef)) {
      return nullptr;
    }
  }

  return actor;
}

ServiceWorkerChild::ServiceWorkerChild()
    : mOwner(nullptr), mTeardownStarted(false) {}

void ServiceWorkerChild::SetOwner(ServiceWorker* aOwner) {
  MOZ_DIAGNOSTIC_ASSERT(!mOwner);
  MOZ_DIAGNOSTIC_ASSERT(aOwner);
  mOwner = aOwner;
}

void ServiceWorkerChild::RevokeOwner(ServiceWorker* aOwner) {
  MOZ_DIAGNOSTIC_ASSERT(mOwner);
  MOZ_DIAGNOSTIC_ASSERT(aOwner == mOwner);
  mOwner = nullptr;
}

void ServiceWorkerChild::MaybeStartTeardown() {
  if (mTeardownStarted) {
    return;
  }
  mTeardownStarted = true;
  Unused << SendTeardown();
}

}  // namespace mozilla::dom
