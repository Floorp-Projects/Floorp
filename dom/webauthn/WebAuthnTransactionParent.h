/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnTransactionParent_h
#define mozilla_dom_WebAuthnTransactionParent_h

#include "mozilla/dom/PWebAuthnTransactionParent.h"

/*
 * Parent process IPC implementation for WebAuthn and U2F API. Receives
 * authentication data to be either registered or signed by a key, passes
 * information to U2FTokenManager.
 */

namespace mozilla {
namespace dom {

class WebAuthnTransactionParent final : public PWebAuthnTransactionParent
{
public:
  NS_INLINE_DECL_REFCOUNTING(WebAuthnTransactionParent);
  WebAuthnTransactionParent() = default;
  virtual mozilla::ipc::IPCResult
  RecvRequestRegister(const WebAuthnTransactionInfo& aTransactionInfo) override;
  virtual mozilla::ipc::IPCResult
  RecvRequestSign(const WebAuthnTransactionInfo& aTransactionInfo) override;
  virtual mozilla::ipc::IPCResult RecvRequestCancel() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
private:
  ~WebAuthnTransactionParent() = default;
};

}
}

#endif //mozilla_dom_WebAuthnTransactionParent_h
