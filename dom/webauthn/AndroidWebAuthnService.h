/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AndroidWebAuthnService_h_
#define mozilla_dom_AndroidWebAuthnService_h_

#include "mozilla/java/WebAuthnTokenManagerNatives.h"
#include "nsIWebAuthnService.h"

namespace mozilla {

namespace dom {

// Collected from
// https://developers.google.com/android/reference/com/google/android/gms/fido/fido2/api/common/ErrorCode
constexpr auto kSecurityError = u"SECURITY_ERR"_ns;
constexpr auto kConstraintError = u"CONSTRAINT_ERR"_ns;
constexpr auto kNotSupportedError = u"NOT_SUPPORTED_ERR"_ns;
constexpr auto kInvalidStateError = u"INVALID_STATE_ERR"_ns;
constexpr auto kNotAllowedError = u"NOT_ALLOWED_ERR"_ns;
constexpr auto kAbortError = u"ABORT_ERR"_ns;
constexpr auto kEncodingError = u"ENCODING_ERR"_ns;
constexpr auto kDataError = u"DATA_ERR"_ns;
constexpr auto kTimeoutError = u"TIMEOUT_ERR"_ns;
constexpr auto kNetworkError = u"NETWORK_ERR"_ns;
constexpr auto kUnknownError = u"UNKNOWN_ERR"_ns;

class AndroidWebAuthnError {
 public:
  explicit AndroidWebAuthnError(const nsAString& aErrorCode)
      : mErrorCode(aErrorCode) {}

  nsresult GetError() const {
    if (mErrorCode.Equals(kSecurityError)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    } else if (mErrorCode.Equals(kConstraintError)) {
      // TODO: The message is right, but it's not about indexeddb.
      // See https://heycam.github.io/webidl/#constrainterror
      return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
    } else if (mErrorCode.Equals(kNotSupportedError)) {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    } else if (mErrorCode.Equals(kInvalidStateError)) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    } else if (mErrorCode.Equals(kNotAllowedError)) {
      return NS_ERROR_DOM_NOT_ALLOWED_ERR;
    } else if (mErrorCode.Equals(kEncodingError)) {
      return NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR;
    } else if (mErrorCode.Equals(kDataError)) {
      return NS_ERROR_DOM_DATA_ERR;
    } else if (mErrorCode.Equals(kTimeoutError)) {
      return NS_ERROR_DOM_TIMEOUT_ERR;
    } else if (mErrorCode.Equals(kNetworkError)) {
      return NS_ERROR_DOM_NETWORK_ERR;
    } else if (mErrorCode.Equals(kAbortError)) {
      return NS_ERROR_DOM_ABORT_ERR;
    } else if (mErrorCode.Equals(kUnknownError)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    } else {
      __android_log_print(ANDROID_LOG_ERROR, "Gecko",
                          "RegisterAbort unknown code: %s",
                          NS_ConvertUTF16toUTF8(mErrorCode).get());
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }
  }

 private:
  const nsString mErrorCode;
};

class AndroidWebAuthnService final : public nsIWebAuthnService {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNSERVICE

  AndroidWebAuthnService() = default;

 private:
  ~AndroidWebAuthnService() = default;

  // The Android FIDO2 API doesn't accept the credProps extension. However, the
  // appropriate value for CredentialPropertiesOutput.rk can be determined
  // entirely from the input, so we cache it here until mRegisterPromise
  // resolves.
  Maybe<bool> mRegisterCredPropsRk;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_AndroidWebAuthnService_h_
