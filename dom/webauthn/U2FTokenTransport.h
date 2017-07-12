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

class U2FRegisterResult {
public:
  explicit U2FRegisterResult(nsTArray<uint8_t>&& aRegistration)
    : mRegistration(aRegistration)
  { }

  void ConsumeRegistration(nsTArray<uint8_t>& aBuffer) {
    aBuffer = Move(mRegistration);
  }

private:
  nsTArray<uint8_t> mRegistration;
};

class U2FSignResult {
public:
  explicit U2FSignResult(nsTArray<uint8_t>&& aKeyHandle,
                         nsTArray<uint8_t>&& aSignature)
    : mKeyHandle(aKeyHandle)
    , mSignature(aSignature)
  { }

  void ConsumeKeyHandle(nsTArray<uint8_t>& aBuffer) {
    aBuffer = Move(mKeyHandle);
  }

  void ConsumeSignature(nsTArray<uint8_t>& aBuffer) {
    aBuffer = Move(mSignature);
  }

private:
  nsTArray<uint8_t> mKeyHandle;
  nsTArray<uint8_t> mSignature;
};

typedef MozPromise<U2FRegisterResult, nsresult, true> U2FRegisterPromise;
typedef MozPromise<U2FSignResult, nsresult, true> U2FSignPromise;

class U2FTokenTransport
{
public:
  NS_INLINE_DECL_REFCOUNTING(U2FTokenTransport);
  U2FTokenTransport() {}

  virtual RefPtr<U2FRegisterPromise>
  Register(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors,
           const nsTArray<uint8_t>& aApplication,
           const nsTArray<uint8_t>& aChallenge) = 0;

  virtual RefPtr<U2FSignPromise>
  Sign(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors,
       const nsTArray<uint8_t>& aApplication,
       const nsTArray<uint8_t>& aChallenge) = 0;

  virtual void Cancel() = 0;

protected:
  virtual ~U2FTokenTransport() = default;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2FTokenTransport_h
