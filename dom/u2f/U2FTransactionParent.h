/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FTransactionParent_h
#define mozilla_dom_U2FTransactionParent_h

#include "mozilla/dom/PWebAuthnTransactionParent.h"

/*
 * Parent process IPC implementation for WebAuthn and U2F API. Receives
 * authentication data to be either registered or signed by a key, passes
 * information to U2FTokenManager.
 */

namespace mozilla {
namespace dom {

class U2FTransactionParent final : public PWebAuthnTransactionParent
{
public:
  NS_INLINE_DECL_REFCOUNTING(U2FTransactionParent);
  U2FTransactionParent() = default;

  virtual mozilla::ipc::IPCResult
  RecvRequestRegister(const uint64_t& aTransactionId,
                      const WebAuthnMakeCredentialInfo& aTransactionInfo) override;

  virtual mozilla::ipc::IPCResult
  RecvRequestSign(const uint64_t& aTransactionId,
                  const WebAuthnGetAssertionInfo& aTransactionInfo) override;

  virtual mozilla::ipc::IPCResult
  RecvRequestCancel(const uint64_t& aTransactionId) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  ~U2FTransactionParent() = default;
};

}
}

#endif //mozilla_dom_U2FTransactionParent_h
