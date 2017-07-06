/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/dom/WebAuthnManager.h"

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
WebAuthnTransactionChild::RecvConfirmRegister(nsTArray<uint8_t>&& aRegBuffer)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::Get();
  MOZ_ASSERT(mgr);
  mgr->FinishMakeCredential(aRegBuffer);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionChild::RecvConfirmSign(nsTArray<uint8_t>&& aCredentialId,
                                          nsTArray<uint8_t>&& aBuffer)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::Get();
  MOZ_ASSERT(mgr);
  mgr->FinishGetAssertion(aCredentialId, aBuffer);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionChild::RecvCancel(const nsresult& aError)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::Get();
  MOZ_ASSERT(mgr);
  mgr->Cancel(aError);
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
