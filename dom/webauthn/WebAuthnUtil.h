/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnUtil_h
#define mozilla_dom_WebAuthnUtil_h

/*
 * Utility functions used by both WebAuthnManager and U2FTokenManager.
 */

#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/WebAuthenticationBinding.h"

namespace mozilla {
namespace dom {

enum class U2FOperation { Register, Sign };

bool EvaluateAppID(nsPIDOMWindowInner* aParent, const nsString& aOrigin,
                   /* in/out */ nsString& aAppId);

nsresult AssembleAuthenticatorData(const CryptoBuffer& rpIdHashBuf,
                                   const uint8_t flags,
                                   const CryptoBuffer& counterBuf,
                                   const CryptoBuffer& attestationDataBuf,
                                   /* out */ CryptoBuffer& authDataBuf);

nsresult AssembleAttestationObject(const CryptoBuffer& aRpIdHash,
                                   const CryptoBuffer& aPubKeyBuf,
                                   const CryptoBuffer& aKeyHandleBuf,
                                   const CryptoBuffer& aAttestationCertBuf,
                                   const CryptoBuffer& aSignatureBuf,
                                   bool aForceNoneAttestation,
                                   /* out */ CryptoBuffer& aAttestationObjBuf);

nsresult U2FDecomposeSignResponse(const CryptoBuffer& aResponse,
                                  /* out */ uint8_t& aFlags,
                                  /* out */ CryptoBuffer& aCounterBuf,
                                  /* out */ CryptoBuffer& aSignatureBuf);

nsresult U2FDecomposeRegistrationResponse(
    const CryptoBuffer& aResponse,
    /* out */ CryptoBuffer& aPubKeyBuf,
    /* out */ CryptoBuffer& aKeyHandleBuf,
    /* out */ CryptoBuffer& aAttestationCertBuf,
    /* out */ CryptoBuffer& aSignatureBuf);

nsresult U2FDecomposeECKey(const CryptoBuffer& aPubKeyBuf,
                           /* out */ CryptoBuffer& aXcoord,
                           /* out */ CryptoBuffer& aYcoord);

nsresult HashCString(const nsACString& aIn, /* out */ CryptoBuffer& aOut);

nsresult BuildTransactionHashes(const nsCString& aRpId,
                                const nsCString& aClientDataJSON,
                                /* out */ CryptoBuffer& aRpIdHash,
                                /* out */ CryptoBuffer& aClientDataHash);

}  // namespace dom
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::AuthenticatorAttachment>
    : public ContiguousEnumSerializer<
          mozilla::dom::AuthenticatorAttachment,
          mozilla::dom::AuthenticatorAttachment::Platform,
          mozilla::dom::AuthenticatorAttachment::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::UserVerificationRequirement>
    : public ContiguousEnumSerializer<
          mozilla::dom::UserVerificationRequirement,
          mozilla::dom::UserVerificationRequirement::Required,
          mozilla::dom::UserVerificationRequirement::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::AttestationConveyancePreference>
    : public ContiguousEnumSerializer<
          mozilla::dom::AttestationConveyancePreference,
          mozilla::dom::AttestationConveyancePreference::None,
          mozilla::dom::AttestationConveyancePreference::EndGuard_> {};

}  // namespace IPC

#endif  // mozilla_dom_WebAuthnUtil_h
