/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnManagerBase_h
#define mozilla_dom_WebAuthnManagerBase_h

/*
 * A base class used by WebAuthn and U2F implementations, providing shared
 * functionality and requiring an interface used by the IPC child actors.
 */

namespace mozilla {
namespace dom {

class WebAuthnManagerBase
{
public:
  virtual void
  FinishMakeCredential(const uint64_t& aTransactionId,
                       nsTArray<uint8_t>& aRegBuffer) = 0;

  virtual void
  FinishGetAssertion(const uint64_t& aTransactionId,
                     nsTArray<uint8_t>& aCredentialId,
                     nsTArray<uint8_t>& aSigBuffer) = 0;

  virtual void
  RequestAborted(const uint64_t& aTransactionId,
                 const nsresult& aError) = 0;

  virtual void ActorDestroyed() = 0;
};

}
}

#endif //mozilla_dom_WebAuthnManagerBase_h
