/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FSoftTokenManager_h
#define mozilla_dom_U2FSoftTokenManager_h

#include "mozilla/dom/U2FTokenTransport.h"
#include "ScopedNSSTypes.h"
#include "nsNSSShutDown.h"

/*
 * U2FSoftTokenManager is a software implementation of a secure token manager
 * for the U2F and WebAuthn APIs.
 */

namespace mozilla {
namespace dom {

class U2FSoftTokenManager final : public U2FTokenTransport,
                                  public nsNSSShutDownObject
{
public:
  explicit U2FSoftTokenManager(uint32_t aCounter);
  virtual nsresult Register(nsTArray<uint8_t>& aApplication,
                            nsTArray<uint8_t>& aChallenge,
                            /* out */ nsTArray<uint8_t>& aRegistration,
                            /* out */ nsTArray<uint8_t>& aSignature) override;
  virtual nsresult Sign(nsTArray<uint8_t>& aApplication,
                        nsTArray<uint8_t>& aChallenge,
                        nsTArray<uint8_t>& aKeyHandle,
                        /* out */ nsTArray<uint8_t>& aSignature) override;
  nsresult IsRegistered(nsTArray<uint8_t>& aKeyHandle,
                        nsTArray<uint8_t>& aAppParam,
                        bool& aResult);

  // For nsNSSShutDownObject
  virtual void virtualDestroyNSSReference() override;
  void destructorSafeDestroyNSSReference();

private:
  ~U2FSoftTokenManager();
  nsresult Init();
  bool IsCompatibleVersion(const nsAString& aVersion);

  bool mInitialized;
  mozilla::UniquePK11SymKey mWrappingKey;

  static const nsCString mSecretNickname;
  static const nsString mVersion;

  nsresult GetOrCreateWrappingKey(const mozilla::UniquePK11SlotInfo& aSlot,
                                  const nsNSSShutDownPreventionLock&);
  uint32_t mCounter;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2FSoftTokenManager_h
