/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManagerChild.h"

#include "ClientHandleChild.h"
#include "ClientManagerOpChild.h"
#include "ClientNavigateOpChild.h"
#include "ClientSourceChild.h"

#include "mozilla/dom/WorkerRef.h"

namespace mozilla {
namespace dom {

void ClientManagerChild::ActorDestroy(ActorDestroyReason aReason) {
  mIPCWorkerRef = nullptr;

  if (mManager) {
    mManager->RevokeActor(this);

    // Revoking the actor link should automatically cause the owner
    // to call RevokeOwner() as well.
    MOZ_DIAGNOSTIC_ASSERT(!mManager);
  }
}

PClientHandleChild* ClientManagerChild::AllocPClientHandleChild(
    const IPCClientInfo& aClientInfo) {
  return new ClientHandleChild();
}

bool ClientManagerChild::DeallocPClientHandleChild(PClientHandleChild* aActor) {
  delete aActor;
  return true;
}

PClientManagerOpChild* ClientManagerChild::AllocPClientManagerOpChild(
    const ClientOpConstructorArgs& aArgs) {
  MOZ_ASSERT_UNREACHABLE(
      "ClientManagerOpChild must be explicitly constructed.");
  return nullptr;
}

bool ClientManagerChild::DeallocPClientManagerOpChild(
    PClientManagerOpChild* aActor) {
  delete aActor;
  return true;
}

PClientNavigateOpChild* ClientManagerChild::AllocPClientNavigateOpChild(
    const ClientNavigateOpConstructorArgs& aArgs) {
  return new ClientNavigateOpChild();
}

bool ClientManagerChild::DeallocPClientNavigateOpChild(
    PClientNavigateOpChild* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult ClientManagerChild::RecvPClientNavigateOpConstructor(
    PClientNavigateOpChild* aActor,
    const ClientNavigateOpConstructorArgs& aArgs) {
  auto actor = static_cast<ClientNavigateOpChild*>(aActor);
  actor->Init(aArgs);
  return IPC_OK();
}

PClientSourceChild* ClientManagerChild::AllocPClientSourceChild(
    const ClientSourceConstructorArgs& aArgs) {
  return new ClientSourceChild(aArgs);
}

bool ClientManagerChild::DeallocPClientSourceChild(PClientSourceChild* aActor) {
  delete aActor;
  return true;
}

// static
ClientManagerChild* ClientManagerChild::Create() {
  ClientManagerChild* actor = new ClientManagerChild();

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_DIAGNOSTIC_ASSERT(workerPrivate);

    RefPtr<IPCWorkerRefHelper<ClientManagerChild>> helper =
        new IPCWorkerRefHelper<ClientManagerChild>(actor);

    actor->mIPCWorkerRef = IPCWorkerRef::Create(
        workerPrivate, "ClientManagerChild",
        [helper] { helper->Actor()->MaybeStartTeardown(); });

    if (NS_WARN_IF(!actor->mIPCWorkerRef)) {
      delete actor;
      return nullptr;
    }
  }

  return actor;
}

ClientManagerChild::ClientManagerChild()
    : mManager(nullptr), mTeardownStarted(false) {}

void ClientManagerChild::SetOwner(ClientThing<ClientManagerChild>* aThing) {
  MOZ_DIAGNOSTIC_ASSERT(aThing);
  MOZ_DIAGNOSTIC_ASSERT(!mManager);
  mManager = aThing;
}

void ClientManagerChild::RevokeOwner(ClientThing<ClientManagerChild>* aThing) {
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  MOZ_DIAGNOSTIC_ASSERT(mManager == aThing);
  mManager = nullptr;
}

void ClientManagerChild::MaybeStartTeardown() {
  if (mTeardownStarted) {
    return;
  }
  mTeardownStarted = true;
  SendTeardown();
}

WorkerPrivate* ClientManagerChild::GetWorkerPrivate() const {
  if (!mIPCWorkerRef) {
    return nullptr;
  }
  return mIPCWorkerRef->Private();
}

}  // namespace dom
}  // namespace mozilla
