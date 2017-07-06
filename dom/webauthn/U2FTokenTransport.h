/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FTokenTransport_h
#define mozilla_dom_U2FTokenTransport_h

#include "mozilla/dom/PWebAuthnTransaction.h"

/*
 * Abstract class representing a transport manager for U2F Keys (software,
 * bluetooth, usb, etc.). Hides the implementation details for specific key
 * transport types.
 */

namespace mozilla {
namespace dom {

typedef MozPromise<nsresult, nsresult, false> ResultPromise;

class U2FTokenTransport
{
public:
  NS_INLINE_DECL_REFCOUNTING(U2FTokenTransport);
  U2FTokenTransport() {}

  virtual RefPtr<ResultPromise>
  Register(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors,
           const nsTArray<uint8_t>& aApplication,
           const nsTArray<uint8_t>& aChallenge,
           /* out */ nsTArray<uint8_t>& aRegistration,
           /* out */ nsTArray<uint8_t>& aSignature) = 0;

  virtual RefPtr<ResultPromise>
  Sign(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors,
       const nsTArray<uint8_t>& aApplication,
       const nsTArray<uint8_t>& aChallenge,
       /* out */ nsTArray<uint8_t>& aKeyHandle,
       /* out */ nsTArray<uint8_t>& aSignature) = 0;

  virtual void Cancel() = 0;

protected:
  virtual ~U2FTokenTransport() = default;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2FTokenTransport_h
