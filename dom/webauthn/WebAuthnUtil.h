/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnUtil_h
#define mozilla_dom_WebAuthnUtil_h

/*
 * Utility functions used by both WebAuthnManager and U2FTokenManager.
 */

#include "mozilla/dom/CryptoBuffer.h"
#include "pkix/Input.h"

namespace mozilla {
namespace dom {
nsresult
U2FAssembleAuthenticatorData(/* out */ CryptoBuffer& aAuthenticatorData,
                             const CryptoBuffer& aRpIdHash,
                             const CryptoBuffer& aSignatureData);

nsresult
U2FDecomposeRegistrationResponse(const CryptoBuffer& aResponse,
                                 /* out */ CryptoBuffer& aPubKeyBuf,
                                 /* out */ CryptoBuffer& aKeyHandleBuf,
                                 /* out */ CryptoBuffer& aAttestationCertBuf,
                                 /* out */ CryptoBuffer& aSignatureBuf);

nsresult
ReadToCryptoBuffer(pkix::Reader& aSrc, /* out */ CryptoBuffer& aDest,
                   uint32_t aLen);

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebAuthnUtil_h
