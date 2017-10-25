/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnTransactionChild.h"

namespace mozilla {
namespace dom {

WebAuthnTransactionChild::WebAuthnTransactionChild()
{
  // Retain a reference so the task object isn't deleted without IPDL's
  // knowledge. The reference will be released by
  // mozilla::ipc::BackgroundChildImpl::DeallocPWebAuthnTransactionChild.
  NS_ADDREF_THIS();
}

mozilla::ipc::IPCResult
WebAuthnTransactionChild::RecvConfirmRegister(const uint64_t& aTransactionId,
                                              nsTArray<uint8_t>&& aRegBuffer)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::Get();
  MOZ_ASSERT(mgr);
  mgr->FinishMakeCredential(aTransactionId, aRegBuffer);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionChild::RecvConfirmSign(const uint64_t& aTransactionId,
                                          nsTArray<uint8_t>&& aCredentialId,
                                          nsTArray<uint8_t>&& aBuffer)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::Get();
  MOZ_ASSERT(mgr);
  mgr->FinishGetAssertion(aTransactionId, aCredentialId, aBuffer);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionChild::RecvAbort(const uint64_t& aTransactionId,
                                    const nsresult& aError)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::Get();
  MOZ_ASSERT(mgr);
  mgr->RequestAborted(aTransactionId, aError);
  return IPC_OK();
}

void
WebAuthnTransactionChild::ActorDestroy(ActorDestroyReason why)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::Get();
  // This could happen after the WebAuthnManager has been shut down.
  if (mgr) {
    mgr->ActorDestroyed();
  }
}

}
}
