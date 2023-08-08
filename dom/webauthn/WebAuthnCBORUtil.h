/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnCBORUtil_h
#define mozilla_dom_WebAuthnCBORUtil_h

/*
 * Serialize and deserialize CBOR data formats for WebAuthn
 */

#include "mozilla/dom/CryptoBuffer.h"

namespace mozilla::dom {

nsresult CBOREncodePublicKeyObj(const CryptoBuffer& aPubKeyBuf,
                                /* out */ CryptoBuffer& aPubKeyObj);

nsresult CBOREncodeFidoU2FAttestationObj(
    const CryptoBuffer& aAuthDataBuf, const CryptoBuffer& aAttestationCertBuf,
    const CryptoBuffer& aSignatureBuf,
    /* out */ CryptoBuffer& aAttestationObj);

nsresult CBOREncodeNoneAttestationObj(const CryptoBuffer& aAuthDataBuf,
                                      /* out */ CryptoBuffer& aAttestationObj);

}  // namespace mozilla::dom
#endif  // mozilla_dom_WebAuthnCBORUtil_h
