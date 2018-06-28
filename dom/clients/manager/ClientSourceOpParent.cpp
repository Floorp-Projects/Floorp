/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSourceOpParent.h"

#include "ClientSourceParent.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;

void
ClientSourceOpParent::ActorDestroy(ActorDestroyReason aReason)
{
  if (mPromise) {
    mPromise->Reject(NS_ERROR_ABORT, __func__);
    mPromise = nullptr;
  }
}

IPCResult
ClientSourceOpParent::Recv__delete__(const ClientOpResult& aResult)
{
  if (aResult.type() == ClientOpResult::Tnsresult &&
      NS_FAILED(aResult.get_nsresult())) {

    // If a control message fails then clear the controller from
    // the ClientSourceParent.  We eagerly marked it controlled at
    // the start of the operation.
    if (mArgs.type() == ClientOpConstructorArgs::TClientControlledArgs) {
      auto source = static_cast<ClientSourceParent*>(Manager());
      if (source) {
        source->ClearController();
      }
    }

    mPromise->Reject(aResult.get_nsresult(), __func__);
    mPromise = nullptr;
    return IPC_OK();
  }

  mPromise->Resolve(aResult, __func__);
  mPromise = nullptr;
  return IPC_OK();
}

ClientSourceOpParent::ClientSourceOpParent(const ClientOpConstructorArgs& aArgs,
                                           ClientOpPromise::Private* aPromise)
  : mArgs(aArgs)
  , mPromise(aPromise)
{
  MOZ_DIAGNOSTIC_ASSERT(mPromise);
}

ClientSourceOpParent::~ClientSourceOpParent()
{
  MOZ_DIAGNOSTIC_ASSERT(!mPromise);
}

} // namespace dom
} // namespace mozilla
