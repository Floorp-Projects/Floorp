/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FHIDTokenManager_h
#define mozilla_dom_U2FHIDTokenManager_h

#include "mozilla/dom/U2FTokenTransport.h"

/*
 * U2FHIDTokenManager is a Rust implementation of a secure token manager
 * for the U2F and WebAuthn APIs, talking to HIDs.
 */

namespace mozilla {
namespace dom {

class U2FHIDTokenManager final : public U2FTokenTransport
{
public:
  explicit U2FHIDTokenManager();

  virtual RefPtr<U2FRegisterPromise>
  Register(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors,
           const nsTArray<uint8_t>& aApplication,
           const nsTArray<uint8_t>& aChallenge,
           uint32_t aTimeoutMS) override;

  virtual RefPtr<U2FSignPromise>
  Sign(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors,
       const nsTArray<uint8_t>& aApplication,
       const nsTArray<uint8_t>& aChallenge,
       uint32_t aTimeoutMS) override;

  void Cancel() override;

private:
  ~U2FHIDTokenManager();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2FHIDTokenManager_h
