/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FSoftTokenTransport_h
#define mozilla_dom_U2FSoftTokenTransport_h

#include "nsIWebAuthnController.h"
#include "ScopedNSSTypes.h"

/*
 * U2FSoftTokenManager is a software implementation of a secure token manager
 * for the U2F and WebAuthn APIs.
 */

namespace mozilla::dom {

class U2FSoftTokenTransport final : public nsIWebAuthnTransport {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBAUTHNTRANSPORT

  explicit U2FSoftTokenTransport(uint32_t aCounter);

 private:
  ~U2FSoftTokenTransport() = default;
  nsresult Init();

  nsresult IsRegistered(const nsTArray<uint8_t>& aKeyHandle,
                        const nsTArray<uint8_t>& aAppParam, bool& aResult);

  bool FindRegisteredKeyHandle(
      const nsTArray<nsTArray<uint8_t>>& aAppIds,
      const nsTArray<nsTArray<uint8_t>>& aCredentialIds,
      /*out*/ nsTArray<uint8_t>& aKeyHandle,
      /*out*/ nsTArray<uint8_t>& aAppId);

  bool mInitialized;
  mozilla::UniquePK11SymKey mWrappingKey;

  static const nsCString mSecretNickname;

  nsresult GetOrCreateWrappingKey(const mozilla::UniquePK11SlotInfo& aSlot);
  uint32_t mCounter;

  nsCOMPtr<nsIWebAuthnController> mController;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_U2FSoftTokenTransport_h
