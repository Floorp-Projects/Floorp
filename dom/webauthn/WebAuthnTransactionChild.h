/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnTransactionChild_h
#define mozilla_dom_WebAuthnTransactionChild_h

#include "mozilla/dom/PWebAuthnTransactionChild.h"

/*
 * Child process IPC implementation for WebAuthn API. Receives results of
 * WebAuthn transactions from the parent process, and sends them to the
 * WebAuthnManager either cancel the transaction, or be formatted and relayed to
 * content.
 */

namespace mozilla {
namespace dom {

class WebAuthnTransactionChild final : public PWebAuthnTransactionChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(WebAuthnTransactionChild);
  WebAuthnTransactionChild();
  mozilla::ipc::IPCResult RecvConfirmRegister(nsTArray<uint8_t>&& aRegBuffer,
                                              nsTArray<uint8_t>&& aSigBuffer) override;
  mozilla::ipc::IPCResult RecvConfirmSign(nsTArray<uint8_t>&& aCredentialId,
                                          nsTArray<uint8_t>&& aBuffer) override;
  mozilla::ipc::IPCResult RecvCancel(const nsresult& aError) override;
  void ActorDestroy(ActorDestroyReason why) override;
private:
  ~WebAuthnTransactionChild() = default;
};

}
}

#endif //mozilla_dom_WebAuthnTransactionChild_h
