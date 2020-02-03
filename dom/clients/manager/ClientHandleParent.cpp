/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientHandleParent.h"

#include "ClientHandleOpParent.h"
#include "ClientManagerService.h"
#include "ClientSourceParent.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;

IPCResult ClientHandleParent::RecvTeardown() {
  Unused << Send__delete__(this);
  return IPC_OK();
}

void ClientHandleParent::ActorDestroy(ActorDestroyReason aReason) {
  if (mSource) {
    mSource->DetachHandle(this);
    mSource = nullptr;
  } else {
    mService->StopWaitingForSource(this, mClientId);
  }

  if (mSourcePromise) {
    CopyableErrorResult rv;
    rv.ThrowAbortError("Client aborted");
    mSourcePromise->Reject(rv, __func__);
  }
}

PClientHandleOpParent* ClientHandleParent::AllocPClientHandleOpParent(
    const ClientOpConstructorArgs& aArgs) {
  return new ClientHandleOpParent();
}

bool ClientHandleParent::DeallocPClientHandleOpParent(
    PClientHandleOpParent* aActor) {
  delete aActor;
  return true;
}

IPCResult ClientHandleParent::RecvPClientHandleOpConstructor(
    PClientHandleOpParent* aActor, const ClientOpConstructorArgs& aArgs) {
  auto actor = static_cast<ClientHandleOpParent*>(aActor);
  actor->Init(std::move(const_cast<ClientOpConstructorArgs&>(aArgs)));
  return IPC_OK();
}

ClientHandleParent::ClientHandleParent()
    : mService(ClientManagerService::GetOrCreateInstance()), mSource(nullptr) {}

ClientHandleParent::~ClientHandleParent() { MOZ_DIAGNOSTIC_ASSERT(!mSource); }

void ClientHandleParent::Init(const IPCClientInfo& aClientInfo) {
  mClientId = aClientInfo.id();
  mPrincipalInfo = aClientInfo.principalInfo();
  mSource = mService->FindSource(aClientInfo.id(), aClientInfo.principalInfo());
  if (!mSource) {
    mService->WaitForSource(this, aClientInfo.id());
    return;
  }

  mSource->AttachHandle(this);
}

ClientSourceParent* ClientHandleParent::GetSource() const { return mSource; }

RefPtr<SourcePromise> ClientHandleParent::EnsureSource() {
  if (mSource) {
    return SourcePromise::CreateAndResolve(mSource, __func__);
  }

  if (!mSourcePromise) {
    mSourcePromise = new SourcePromise::Private(__func__);
  }
  return mSourcePromise;
}

void ClientHandleParent::FoundSource(ClientSourceParent* aSource) {
  MOZ_ASSERT(aSource->Info().Id() == mClientId);
  if (!ClientMatchPrincipalInfo(aSource->Info().PrincipalInfo(),
                                mPrincipalInfo)) {
    if (mSourcePromise) {
      CopyableErrorResult rv;
      rv.ThrowAbortError("Client aborted");
      mSourcePromise->Reject(rv, __func__);
    }
    Unused << Send__delete__(this);
    return;
  }

  mSource = aSource;
  mSource->AttachHandle(this);
  if (mSourcePromise) {
    mSourcePromise->Resolve(aSource, __func__);
  }
}

}  // namespace dom
}  // namespace mozilla
