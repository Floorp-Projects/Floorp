/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AndroidWebAuthnTokenManager_h
#define mozilla_dom_AndroidWebAuthnTokenManager_h

#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/U2FTokenTransport.h"

namespace mozilla {
namespace dom {

// Collected from
// https://developers.google.com/android/reference/com/google/android/gms/fido/fido2/api/common/ErrorCode
NS_NAMED_LITERAL_STRING(kSecurityError, "SECURITY_ERR");
NS_NAMED_LITERAL_STRING(kConstraintError, "CONSTRAINT_ERR");
NS_NAMED_LITERAL_STRING(kNotSupportedError, "NOT_SUPPORTED_ERR");
NS_NAMED_LITERAL_STRING(kInvalidStateError, "INVALID_STATE_ERR");
NS_NAMED_LITERAL_STRING(kNotAllowedError, "NOT_ALLOWED_ERR");
NS_NAMED_LITERAL_STRING(kAbortError, "ABORT_ERR");
NS_NAMED_LITERAL_STRING(kEncodingError, "ENCODING_ERR");
NS_NAMED_LITERAL_STRING(kDataError, "DATA_ERR");
NS_NAMED_LITERAL_STRING(kTimeoutError, "TIMEOUT_ERR");
NS_NAMED_LITERAL_STRING(kNetworkError, "NETWORK_ERR");
NS_NAMED_LITERAL_STRING(kUnknownError, "UNKNOWN_ERR");

class AndroidWebAuthnResult {
 public:
  explicit AndroidWebAuthnResult(const nsAString& aErrorCode)
      : mErrorCode(aErrorCode) {}

  explicit AndroidWebAuthnResult() {}

  bool IsError() const { return NS_FAILED(GetError()); }

  nsresult GetError() const {
    if (mErrorCode.IsEmpty()) {
      return NS_OK;
    } else if (mErrorCode.Equals(kSecurityError)) {
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

  // Attestation-only
  CryptoBuffer mAttObj;

  // Attestations and assertions
  CryptoBuffer mKeyHandle;
  nsCString mClientDataJSON;

  // Assertions-only
  CryptoBuffer mAuthData;
  CryptoBuffer mSignature;
  CryptoBuffer mUserHandle;

 private:
  const nsString mErrorCode;
};

/*
 * WebAuthnAndroidTokenManager is a token implementation communicating with
 * Android Fido2 APIs.
 */
class AndroidWebAuthnTokenManager final : public U2FTokenTransport {
 public:
  explicit AndroidWebAuthnTokenManager();
  ~AndroidWebAuthnTokenManager() {}

  RefPtr<U2FRegisterPromise> Register(const WebAuthnMakeCredentialInfo& aInfo,
                                      bool aForceNoneAttestation) override;

  RefPtr<U2FSignPromise> Sign(const WebAuthnGetAssertionInfo& aInfo) override;

  void Cancel() override;

  void Drop() override;

  void HandleRegisterResult(const AndroidWebAuthnResult& aResult);

  void HandleSignResult(const AndroidWebAuthnResult& aResult);

  static AndroidWebAuthnTokenManager* GetInstance();

 private:
  void ClearPromises() {
    mRegisterPromise.RejectIfExists(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
    mSignPromise.RejectIfExists(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
  }

  void AssertIsOnOwningThread() const;

  MozPromiseHolder<U2FRegisterPromise> mRegisterPromise;
  MozPromiseHolder<U2FSignPromise> mSignPromise;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_AndroidWebAuthnTokenManager_h
