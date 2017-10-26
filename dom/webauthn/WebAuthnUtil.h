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
#include "pkix/Input.h"

namespace mozilla {
namespace dom {
nsresult
AssembleAuthenticatorData(const CryptoBuffer& rpIdHashBuf,
                          const uint8_t flags,
                          const CryptoBuffer& counterBuf,
                          const CryptoBuffer& attestationDataBuf,
                          /* out */ CryptoBuffer& authDataBuf);

nsresult
AssembleAttestationData(const CryptoBuffer& aaguidBuf,
                        const CryptoBuffer& keyHandleBuf,
                        const CryptoBuffer& pubKeyObj,
                        /* out */ CryptoBuffer& attestationDataBuf);

nsresult
U2FDecomposeSignResponse(const CryptoBuffer& aResponse,
                         /* out */ uint8_t& aFlags,
                         /* out */ CryptoBuffer& aCounterBuf,
                         /* out */ CryptoBuffer& aSignatureBuf);

nsresult
U2FDecomposeRegistrationResponse(const CryptoBuffer& aResponse,
                                 /* out */ CryptoBuffer& aPubKeyBuf,
                                 /* out */ CryptoBuffer& aKeyHandleBuf,
                                 /* out */ CryptoBuffer& aAttestationCertBuf,
                                 /* out */ CryptoBuffer& aSignatureBuf);

nsresult
ReadToCryptoBuffer(pkix::Reader& aSrc, /* out */ CryptoBuffer& aDest,
                   uint32_t aLen);

nsresult
U2FDecomposeECKey(const CryptoBuffer& aPubKeyBuf,
                  /* out */ CryptoBuffer& aXcoord,
                  /* out */ CryptoBuffer& aYcoord);

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebAuthnUtil_h
