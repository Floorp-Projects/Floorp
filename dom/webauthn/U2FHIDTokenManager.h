/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FHIDTokenManager_h
#define mozilla_dom_U2FHIDTokenManager_h

#include "mozilla/dom/U2FTokenTransport.h"
#include "u2f-hid-rs/src/u2fhid-capi.h"

/*
 * U2FHIDTokenManager is a Rust implementation of a secure token manager
 * for the U2F and WebAuthn APIs, talking to HIDs.
 */

namespace mozilla {
namespace dom {

class U2FKeyHandles {
public:
  explicit U2FKeyHandles(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors)
  {
    mKeyHandles = rust_u2f_khs_new();

    for (auto desc: aDescriptors) {
      rust_u2f_khs_add(mKeyHandles, desc.id().Elements(), desc.id().Length());
    }
  }

  rust_u2f_key_handles* Get() { return mKeyHandles; }

  ~U2FKeyHandles() { rust_u2f_khs_free(mKeyHandles); }

private:
  rust_u2f_key_handles* mKeyHandles;
};

class U2FResult {
public:
  explicit U2FResult(uint64_t aTransactionId, rust_u2f_result* aResult)
    : mTransactionId(aTransactionId)
    , mResult(aResult)
  { }

  ~U2FResult() { rust_u2f_res_free(mResult); }

  uint64_t GetTransactionId() { return mTransactionId; }

  bool CopyRegistration(nsTArray<uint8_t>& aBuffer)
  {
    return CopyBuffer(U2F_RESBUF_ID_REGISTRATION, aBuffer);
  }

  bool CopyKeyHandle(nsTArray<uint8_t>& aBuffer)
  {
    return CopyBuffer(U2F_RESBUF_ID_KEYHANDLE, aBuffer);
  }

  bool CopySignature(nsTArray<uint8_t>& aBuffer)
  {
    return CopyBuffer(U2F_RESBUF_ID_SIGNATURE, aBuffer);
  }

private:
  bool CopyBuffer(uint8_t aResBufID, nsTArray<uint8_t>& aBuffer) {
    if (!mResult) {
      return false;
    }

    size_t len;
    if (!rust_u2f_resbuf_length(mResult, aResBufID, &len)) {
      return false;
    }

    if (!aBuffer.SetLength(len, fallible)) {
      return false;
    }

    return rust_u2f_resbuf_copy(mResult, aResBufID, aBuffer.Elements());
  }

  uint64_t mTransactionId;
  rust_u2f_result* mResult;
};

class U2FHIDTokenManager final : public U2FTokenTransport
{
public:
  explicit U2FHIDTokenManager();

  virtual RefPtr<U2FRegisterPromise>
  Register(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors,
           const WebAuthnAuthenticatorSelection &aAuthenticatorSelection,
           const nsTArray<uint8_t>& aApplication,
           const nsTArray<uint8_t>& aChallenge,
           uint32_t aTimeoutMS) override;

  virtual RefPtr<U2FSignPromise>
  Sign(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors,
       const nsTArray<uint8_t>& aApplication,
       const nsTArray<uint8_t>& aChallenge,
       uint32_t aTimeoutMS) override;

  void Cancel() override;

  void HandleRegisterResult(UniquePtr<U2FResult>&& aResult);
  void HandleSignResult(UniquePtr<U2FResult>&& aResult);

private:
  ~U2FHIDTokenManager();

  void ClearPromises() {
    mRegisterPromise.RejectIfExists(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
    mSignPromise.RejectIfExists(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
  }

  rust_u2f_manager* mU2FManager;
  uint64_t mTransactionId;
  MozPromiseHolder<U2FRegisterPromise> mRegisterPromise;
  MozPromiseHolder<U2FSignPromise> mSignPromise;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2FHIDTokenManager_h
