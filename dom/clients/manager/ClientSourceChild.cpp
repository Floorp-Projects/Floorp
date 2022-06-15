/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSourceChild.h"

#include "ClientSource.h"
#include "ClientSourceOpChild.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/Unused.h"

namespace mozilla::dom {

using mozilla::ipc::IPCResult;

void ClientSourceChild::ActorDestroy(ActorDestroyReason aReason) {
  if (mSource) {
    mSource->RevokeActor(this);

    // Revoking the actor link should automatically cause the owner
    // to call RevokeOwner() as well.
    MOZ_DIAGNOSTIC_ASSERT(!mSource);
  }
}

PClientSourceOpChild* ClientSourceChild::AllocPClientSourceOpChild(
    const ClientOpConstructorArgs& aArgs) {
  return new ClientSourceOpChild();
}

bool ClientSourceChild::DeallocPClientSourceOpChild(
    PClientSourceOpChild* aActor) {
  static_cast<ClientSourceOpChild*>(aActor)->ScheduleDeletion();
  return true;
}

IPCResult ClientSourceChild::RecvPClientSourceOpConstructor(
    PClientSourceOpChild* aActor, const ClientOpConstructorArgs& aArgs) {
  auto actor = static_cast<ClientSourceOpChild*>(aActor);
  actor->Init(aArgs);
  return IPC_OK();
}

mozilla::ipc::IPCResult ClientSourceChild::RecvEvictFromBFCache() {
  if (mSource) {
    mSource->EvictFromBFCache();
  }

  return IPC_OK();
}

ClientSourceChild::ClientSourceChild(const ClientSourceConstructorArgs& aArgs)
    : mSource(nullptr), mTeardownStarted(false) {}

void ClientSourceChild::SetOwner(ClientThing<ClientSourceChild>* aThing) {
  MOZ_DIAGNOSTIC_ASSERT(aThing);
  MOZ_DIAGNOSTIC_ASSERT(!mSource);
  mSource = static_cast<ClientSource*>(aThing);
}

void ClientSourceChild::RevokeOwner(ClientThing<ClientSourceChild>* aThing) {
  MOZ_DIAGNOSTIC_ASSERT(mSource);
  MOZ_DIAGNOSTIC_ASSERT(mSource == static_cast<ClientSource*>(aThing));
  mSource = nullptr;
}

ClientSource* ClientSourceChild::GetSource() const { return mSource; }

void ClientSourceChild::MaybeStartTeardown() {
  if (mTeardownStarted) {
    return;
  }
  mTeardownStarted = true;
  Unused << SendTeardown();
}

}  // namespace mozilla::dom
