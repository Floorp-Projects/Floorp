/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FSoftTokenManager_h
#define mozilla_dom_U2FSoftTokenManager_h

#include "mozilla/dom/U2FTokenTransport.h"
#include "ScopedNSSTypes.h"

/*
 * U2FSoftTokenManager is a software implementation of a secure token manager
 * for the U2F and WebAuthn APIs.
 */

namespace mozilla {
namespace dom {

class U2FSoftTokenManager final : public U2FTokenTransport
{
public:
  explicit U2FSoftTokenManager(uint32_t aCounter);

  RefPtr<U2FRegisterPromise>
  Register(const WebAuthnMakeCredentialInfo& aInfo,
           bool aForceNoneAttestation) override;

  RefPtr<U2FSignPromise>
  Sign(const WebAuthnGetAssertionInfo& aInfo) override;

  void Cancel() override;

private:
  ~U2FSoftTokenManager() {}
  nsresult Init();

  nsresult IsRegistered(const nsTArray<uint8_t>& aKeyHandle,
                        const nsTArray<uint8_t>& aAppParam,
                        bool& aResult);

  bool
  FindRegisteredKeyHandle(const nsTArray<nsTArray<uint8_t>>& aAppIds,
                          const nsTArray<WebAuthnScopedCredential>& aCredentials,
                          /*out*/ nsTArray<uint8_t>& aKeyHandle,
                          /*out*/ nsTArray<uint8_t>& aAppId);

  bool mInitialized;
  mozilla::UniquePK11SymKey mWrappingKey;

  static const nsCString mSecretNickname;

  nsresult GetOrCreateWrappingKey(const mozilla::UniquePK11SlotInfo& aSlot);
  uint32_t mCounter;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2FSoftTokenManager_h
