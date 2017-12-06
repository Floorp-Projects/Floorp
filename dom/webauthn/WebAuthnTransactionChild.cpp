/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnTransactionChild.h"

namespace mozilla {
namespace dom {

WebAuthnTransactionChild::WebAuthnTransactionChild(WebAuthnManagerBase* aManager)
  : mManager(aManager)
{
  MOZ_ASSERT(aManager);

  // Retain a reference so the task object isn't deleted without IPDL's
  // knowledge. The reference will be released by
  // mozilla::ipc::BackgroundChildImpl::DeallocPWebAuthnTransactionChild.
  NS_ADDREF_THIS();
}

mozilla::ipc::IPCResult
WebAuthnTransactionChild::RecvConfirmRegister(const uint64_t& aTransactionId,
                                              nsTArray<uint8_t>&& aRegBuffer)
{
  if (NS_WARN_IF(!mManager)) {
    return IPC_FAIL_NO_REASON(this);
  }

  mManager->FinishMakeCredential(aTransactionId, aRegBuffer);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionChild::RecvConfirmSign(const uint64_t& aTransactionId,
                                          nsTArray<uint8_t>&& aCredentialId,
                                          nsTArray<uint8_t>&& aBuffer)
{
  if (NS_WARN_IF(!mManager)) {
    return IPC_FAIL_NO_REASON(this);
  }

  mManager->FinishGetAssertion(aTransactionId, aCredentialId, aBuffer);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionChild::RecvAbort(const uint64_t& aTransactionId,
                                    const nsresult& aError)
{
  if (NS_WARN_IF(!mManager)) {
    return IPC_FAIL_NO_REASON(this);
  }

  mManager->RequestAborted(aTransactionId, aError);
  return IPC_OK();
}

void
WebAuthnTransactionChild::ActorDestroy(ActorDestroyReason why)
{
  // Called by either a __delete__ message from the parent, or when the
  // channel disconnects. Clear out the child actor reference to be sure.
  if (mManager) {
    mManager->ActorDestroyed();
    mManager = nullptr;
  }
}

void
WebAuthnTransactionChild::Disconnect()
{
  mManager = nullptr;

  // The WebAuthnManager released us, but we're going to be held alive by the
  // IPC layer. The parent will explicitly destroy us via Send__delete__(),
  // after receiving the DestroyMe message.

  SendDestroyMe();
}

}
}
