/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncWorkerChild.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerScope.h"

namespace mozilla::dom {

VsyncWorkerChild::VsyncWorkerChild() = default;

VsyncWorkerChild::~VsyncWorkerChild() = default;

bool VsyncWorkerChild::Initialize(WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  MOZ_ASSERT(!mWorkerRef);

  mWorkerRef =
      IPCWorkerRef::Create(aWorkerPrivate, "VsyncWorkerChild",
                           [self = RefPtr{this}]() { self->Destroy(); });
  return !!mWorkerRef;
}

void VsyncWorkerChild::Destroy() {
  MOZ_ASSERT_IF(!CanSend(), !mWorkerRef);
  MOZ_ASSERT_IF(!CanSend(), !mObserving);
  Send__delete__(this);
  MOZ_ASSERT(!mWorkerRef);
  MOZ_ASSERT(!mObserving);
}

void VsyncWorkerChild::TryObserve() {
  if (CanSend() && !mObserving) {
    mObserving = SendObserve();
  }
}

void VsyncWorkerChild::TryUnobserve() {
  if (CanSend() && mObserving) {
    mObserving = !SendUnobserve();
  }
}

mozilla::ipc::IPCResult VsyncWorkerChild::RecvNotify(const VsyncEvent& aVsync,
                                                     const float& aVsyncRate) {
  // For MOZ_CAN_RUN_SCRIPT_BOUNDARY purposes: We know IPDL is explicitly
  // keeping our actor alive until it is done processing the messages. We know
  // that the WorkerGlobalScope callee is responsible keeping itself alive
  // during the OnVsync callback.
  WorkerPrivate* workerPrivate = mWorkerRef->Private();
  WorkerGlobalScope* scope = workerPrivate->GlobalScope();
  if (scope) {
    scope->OnVsync(aVsync);
  }
  return IPC_OK();
}

void VsyncWorkerChild::ActorDestroy(ActorDestroyReason aActorDestroyReason) {
  mWorkerRef = nullptr;
  mObserving = false;
}

}  // namespace mozilla::dom
