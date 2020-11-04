/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSourceOpParent.h"

#include "ClientSourceParent.h"

namespace mozilla::dom {

using mozilla::ipc::IPCResult;

void ClientSourceOpParent::ActorDestroy(ActorDestroyReason aReason) {
  if (mPromise) {
    CopyableErrorResult rv;
    rv.ThrowAbortError("Client torn down");
    mPromise->Reject(rv, __func__);
    mPromise = nullptr;
  }
}

IPCResult ClientSourceOpParent::Recv__delete__(const ClientOpResult& aResult) {
  if (aResult.type() == ClientOpResult::TCopyableErrorResult &&
      aResult.get_CopyableErrorResult().Failed()) {
    // If a control message fails then clear the controller from
    // the ClientSourceParent.  We eagerly marked it controlled at
    // the start of the operation.
    if (mArgs.type() == ClientOpConstructorArgs::TClientControlledArgs) {
      auto source = static_cast<ClientSourceParent*>(Manager());
      if (source) {
        source->ClearController();
      }
    }

    mPromise->Reject(aResult.get_CopyableErrorResult(), __func__);
    mPromise = nullptr;
    return IPC_OK();
  }

  mPromise->Resolve(aResult, __func__);
  mPromise = nullptr;
  return IPC_OK();
}

ClientSourceOpParent::ClientSourceOpParent(ClientOpConstructorArgs&& aArgs,
                                           ClientOpPromise::Private* aPromise)
    : mArgs(std::move(aArgs)), mPromise(aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(mPromise);
}

ClientSourceOpParent::~ClientSourceOpParent() {
  MOZ_DIAGNOSTIC_ASSERT(!mPromise);
}

}  // namespace mozilla::dom
