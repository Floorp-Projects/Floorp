/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientNavigateOpParent.h"

namespace mozilla::dom {

using mozilla::ipc::IPCResult;

void ClientNavigateOpParent::ActorDestroy(ActorDestroyReason aReason) {
  if (mPromise) {
    CopyableErrorResult rv;
    rv.ThrowAbortError("Client aborted");
    mPromise->Reject(rv, __func__);
    mPromise = nullptr;
  }
}

IPCResult ClientNavigateOpParent::Recv__delete__(
    const ClientOpResult& aResult) {
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

ClientNavigateOpParent::ClientNavigateOpParent(
    const ClientNavigateOpConstructorArgs& aArgs,
    ClientOpPromise::Private* aPromise)
    : mPromise(aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(mPromise);
}

ClientNavigateOpParent::~ClientNavigateOpParent() {
  MOZ_DIAGNOSTIC_ASSERT(!mPromise);
}

}  // namespace mozilla::dom
