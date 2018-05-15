/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientHandleOpChild.h"

#include "ClientHandle.h"

namespace mozilla {
namespace dom {

void
ClientHandleOpChild::ActorDestroy(ActorDestroyReason aReason)
{
  mClientHandle = nullptr;
  mRejectCallback(NS_ERROR_DOM_ABORT_ERR);
}

mozilla::ipc::IPCResult
ClientHandleOpChild::Recv__delete__(const ClientOpResult& aResult)
{
  mClientHandle = nullptr;
  if (aResult.type() == ClientOpResult::Tnsresult &&
      NS_FAILED(aResult.get_nsresult())) {
    mRejectCallback(aResult.get_nsresult());
    return IPC_OK();
  }
  mResolveCallback(aResult);
  return IPC_OK();
}

ClientHandleOpChild::ClientHandleOpChild(ClientHandle* aClientHandle,
                                         const ClientOpConstructorArgs& aArgs,
                                         const ClientOpCallback&& aResolveCallback,
                                         const ClientOpCallback&& aRejectCallback)
  : mClientHandle(aClientHandle)
  , mResolveCallback(Move(aResolveCallback))
  , mRejectCallback(Move(aRejectCallback))
{
  MOZ_RELEASE_ASSERT(mClientHandle);
  MOZ_RELEASE_ASSERT(mResolveCallback);
  MOZ_RELEASE_ASSERT(mRejectCallback);
}

} // namespace dom
} // namespace mozilla
