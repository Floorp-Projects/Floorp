/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManagerOpChild.h"

#include "mozilla/dom/ClientManager.h"
#include "mozilla/ipc/ProtocolUtils.h"

namespace mozilla::dom {

void ClientManagerOpChild::ActorDestroy(ActorDestroyReason aReason) {
  mClientManager = nullptr;
  if (mPromise) {
    CopyableErrorResult rv;
    rv.ThrowAbortError("Client aborted");
    mPromise->Reject(rv, __func__);
    mPromise = nullptr;
  }
}

mozilla::ipc::IPCResult ClientManagerOpChild::Recv__delete__(
    const ClientOpResult& aResult) {
  mClientManager = nullptr;
  if (aResult.type() == ClientOpResult::TCopyableErrorResult &&
      aResult.get_CopyableErrorResult().Failed()) {
    mPromise->Reject(aResult.get_CopyableErrorResult(), __func__);
    mPromise = nullptr;
    return IPC_OK();
  }
  mPromise->Resolve(aResult, __func__);
  mPromise = nullptr;
  return IPC_OK();
}

ClientManagerOpChild::ClientManagerOpChild(ClientManager* aClientManager,
                                           const ClientOpConstructorArgs& aArgs,
                                           ClientOpPromise::Private* aPromise)
    : mClientManager(aClientManager), mPromise(aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(mClientManager);
  MOZ_DIAGNOSTIC_ASSERT(mPromise);
}

ClientManagerOpChild::~ClientManagerOpChild() {
  MOZ_DIAGNOSTIC_ASSERT(!mPromise);
}

}  // namespace mozilla::dom
