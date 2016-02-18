/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NSSToken_h
#define mozilla_dom_NSSToken_h

#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/Mutex.h"
#include "nsNSSShutDown.h"
#include "ScopedNSSTypes.h"

namespace mozilla {
namespace dom {

// NSSToken will support FIDO U2F operations using NSS for the crypto layer.
// This is a stub. It will be implemented in bug 1244960.
class NSSToken final : public nsNSSShutDownObject
{
public:
  NSSToken();

  ~NSSToken();

  nsresult Init();

  bool IsCompatibleVersion(const nsString& aVersionParam) const;

  bool IsRegistered(const CryptoBuffer& aKeyHandle) const;

  nsresult Register(const CryptoBuffer& aApplicationParam,
                    const CryptoBuffer& aChallengeParam,
                    CryptoBuffer& aRegistrationData);

  nsresult Sign(const CryptoBuffer& aApplicationParam,
                const CryptoBuffer& aChallengeParam,
                const CryptoBuffer& aKeyHandle,
                CryptoBuffer& aSignatureData);

  // For nsNSSShutDownObject
  virtual void virtualDestroyNSSReference() override;
  void destructorSafeDestroyNSSReference();

private:
  bool mInitialized;
  ScopedPK11SlotInfo mSlot;
  mozilla::Mutex mMutex;

  static const nsString mVersion;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_NSSToken_h
