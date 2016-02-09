/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "USBToken.h"

namespace mozilla {
namespace dom {

USBToken::USBToken()
  : mInitialized(false)
{}

USBToken::~USBToken()
{}

nsresult
USBToken::Init()
{
  // This routine does nothing at present, but Bug 1245527 will
  // integrate the upcoming USB HID service here, which will likely
  // require an initialization upon load.
  MOZ_ASSERT(!mInitialized);
  if (mInitialized) {
    return NS_OK;
  }

  mInitialized = true;
  return NS_OK;
}

const nsString USBToken::mVersion = NS_LITERAL_STRING("U2F_V2");

bool
USBToken::IsCompatibleVersion(const nsString& aVersionParam) const
{
  MOZ_ASSERT(mInitialized);
  return mVersion == aVersionParam;
}

bool
USBToken::IsRegistered(const CryptoBuffer& aKeyHandle) const
{
  MOZ_ASSERT(mInitialized);
  return false;
}

nsresult
USBToken::Register(const Optional<Nullable<int32_t>>& opt_timeoutSeconds,
                   const CryptoBuffer& /* aChallengeParam */,
                   const CryptoBuffer& /* aApplicationParam */,
                   CryptoBuffer& aRegistrationData) const
{
  MOZ_ASSERT(mInitialized);
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult
USBToken::Sign(const Optional<Nullable<int32_t>>& opt_timeoutSeconds,
               const CryptoBuffer& aApplicationParam,
               const CryptoBuffer& aChallengeParam,
               const CryptoBuffer& aKeyHandle,
               CryptoBuffer& aSignatureData) const
{
  MOZ_ASSERT(mInitialized);
  return NS_ERROR_NOT_AVAILABLE;
}

} // namespace dom
} // namespace mozilla
