/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_USBToken_h
#define mozilla_dom_USBToken_h

#include "mozilla/dom/CryptoBuffer.h"

namespace mozilla {
namespace dom {

// USBToken implements FIDO operations using a USB device.
class USBToken final
{
public:
  USBToken();

  ~USBToken();

  nsresult Init();

  bool IsCompatibleVersion(const nsString& aVersionParam) const;

  bool IsRegistered(const CryptoBuffer& aKeyHandle) const;

  nsresult Register(const Optional<Nullable<int32_t>>& opt_timeoutSeconds,
                    const CryptoBuffer& aApplicationParam,
                    const CryptoBuffer& aChallengeParam,
                    CryptoBuffer& aRegistrationData) const;

  nsresult Sign(const Optional<Nullable<int32_t>>& opt_timeoutSeconds,
                const CryptoBuffer& aApplicationParam,
                const CryptoBuffer& aChallengeParam,
                const CryptoBuffer& aKeyHandle,
                CryptoBuffer& aSignatureData) const;

private:
  bool mInitialized;

  static const nsString mVersion;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_USBToken_h
