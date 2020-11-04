/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientHandleChild.h"

#include "ClientHandle.h"
#include "ClientHandleOpChild.h"
#include "mozilla/dom/ClientIPCTypes.h"

namespace mozilla::dom {

using mozilla::ipc::IPCResult;

IPCResult ClientHandleChild::RecvExecutionReady(
    const IPCClientInfo& aClientInfo) {
  if (mHandle) {
    mHandle->ExecutionReady(ClientInfo(aClientInfo));
  }
  return IPC_OK();
}

void ClientHandleChild::ActorDestroy(ActorDestroyReason aReason) {
  if (mHandle) {
    mHandle->RevokeActor(this);

    // Revoking the actor link should automatically cause the owner
    // to call RevokeOwner() as well.
    MOZ_DIAGNOSTIC_ASSERT(!mHandle);
  }
}

PClientHandleOpChild* ClientHandleChild::AllocPClientHandleOpChild(
    const ClientOpConstructorArgs& aArgs) {
  MOZ_ASSERT_UNREACHABLE("ClientHandleOpChild must be explicitly constructed.");
  return nullptr;
}

bool ClientHandleChild::DeallocPClientHandleOpChild(
    PClientHandleOpChild* aActor) {
  delete aActor;
  return true;
}

ClientHandleChild::ClientHandleChild()
    : mHandle(nullptr), mTeardownStarted(false) {}

void ClientHandleChild::SetOwner(ClientThing<ClientHandleChild>* aThing) {
  MOZ_DIAGNOSTIC_ASSERT(!mHandle);
  mHandle = static_cast<ClientHandle*>(aThing);
  MOZ_DIAGNOSTIC_ASSERT(mHandle);
}

void ClientHandleChild::RevokeOwner(ClientThing<ClientHandleChild>* aThing) {
  MOZ_DIAGNOSTIC_ASSERT(mHandle);
  MOZ_DIAGNOSTIC_ASSERT(mHandle == static_cast<ClientHandle*>(aThing));
  mHandle = nullptr;
}

void ClientHandleChild::MaybeStartTeardown() {
  if (mTeardownStarted) {
    return;
  }
  mTeardownStarted = true;
  Unused << SendTeardown();
}

}  // namespace mozilla::dom
