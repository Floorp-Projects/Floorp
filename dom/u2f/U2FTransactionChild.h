/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FTransactionChild_h
#define mozilla_dom_U2FTransactionChild_h

#include "mozilla/dom/WebAuthnTransactionChildBase.h"

/*
 * Child process IPC implementation for U2F API. Receives results of U2F
 * transactions from the parent process, and sends them to the U2FManager
 * to either cancel the transaction, or be formatted and relayed to content.
 */

namespace mozilla {
namespace dom {

class U2FTransactionChild final : public WebAuthnTransactionChildBase
{
public:
  explicit U2FTransactionChild(U2F* aU2F) : mU2F(aU2F) {
    MOZ_ASSERT(mU2F);
  }

  mozilla::ipc::IPCResult
  RecvConfirmRegister(const uint64_t& aTransactionId,
                      nsTArray<uint8_t>&& aRegBuffer) override;

  mozilla::ipc::IPCResult
  RecvConfirmSign(const uint64_t& aTransactionId,
                  nsTArray<uint8_t>&& aCredentialId,
                  nsTArray<uint8_t>&& aBuffer) override;

  mozilla::ipc::IPCResult
  RecvAbort(const uint64_t& aTransactionId, const nsresult& aError) override;

  void ActorDestroy(ActorDestroyReason why) override;

private:
  // ~U2F() will destroy child actors.
  U2F* mU2F;
};

}
}

#endif //mozilla_dom_U2FTransactionChild_h
