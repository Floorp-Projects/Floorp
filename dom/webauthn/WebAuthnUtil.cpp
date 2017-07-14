/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnUtil.h"
#include "pkixutil.h"

namespace mozilla {
namespace dom {

nsresult
ReadToCryptoBuffer(pkix::Reader& aSrc, /* out */ CryptoBuffer& aDest,
                   uint32_t aLen)
{
  if (aSrc.EnsureLength(aLen) != pkix::Success) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  if (!aDest.SetCapacity(aLen, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t offset = 0; offset < aLen; ++offset) {
    uint8_t b;
    if (aSrc.Read(b) != pkix::Success) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }
    if (!aDest.AppendElement(b, mozilla::fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

nsresult
U2FAssembleAuthenticatorData(/* out */ CryptoBuffer& aAuthenticatorData,
                             const CryptoBuffer& aRpIdHash,
                             const CryptoBuffer& aSignatureData)
{
  // The AuthenticatorData for U2F devices is the concatenation of the
  // RP ID with the output of the U2F Sign operation.
  if (aRpIdHash.Length() != 32) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aAuthenticatorData.AppendElements(aRpIdHash, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!aAuthenticatorData.AppendElements(aSignatureData, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
U2FDecomposeRegistrationResponse(const CryptoBuffer& aResponse,
                                 /* out */ CryptoBuffer& aPubKeyBuf,
                                 /* out */ CryptoBuffer& aKeyHandleBuf,
                                 /* out */ CryptoBuffer& aAttestationCertBuf,
                                 /* out */ CryptoBuffer& aSignatureBuf)
{
  // U2F v1.1 Format via
  // http://fidoalliance.org/specs/fido-u2f-v1.1-id-20160915/fido-u2f-raw-message-formats-v1.1-id-20160915.html
  //
  // Bytes  Value
  // 1      0x05
  // 65     public key
  // 1      key handle length
  // *      key handle
  // ASN.1  attestation certificate
  // *      attestation signature

  pkix::Input u2fResponse;
  u2fResponse.Init(aResponse.Elements(), aResponse.Length());

  pkix::Reader input(u2fResponse);

  uint8_t b;
  if (input.Read(b) != pkix::Success) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }
  if (b != 0x05) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  nsresult rv = ReadToCryptoBuffer(input, aPubKeyBuf, 65);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint8_t handleLen;
  if (input.Read(handleLen) != pkix::Success) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  rv = ReadToCryptoBuffer(input, aKeyHandleBuf, handleLen);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We have to parse the ASN.1 SEQUENCE on the outside to determine the cert's
  // length.
  pkix::Input cert;
  if (pkix::der::ExpectTagAndGetTLV(input, pkix::der::SEQUENCE, cert)
      != pkix::Success) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  pkix::Reader certInput(cert);
  rv = ReadToCryptoBuffer(certInput, aAttestationCertBuf, cert.GetLength());
  if (NS_FAILED(rv)) {
    return rv;
  }

  // The remainder of u2fResponse is the signature
  pkix::Input u2fSig;
  input.SkipToEnd(u2fSig);
  pkix::Reader sigInput(u2fSig);
  rv = ReadToCryptoBuffer(sigInput, aSignatureBuf, u2fSig.GetLength());
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(input.AtEnd());
  return NS_OK;
}

nsresult
U2FDecomposeECKey(const CryptoBuffer& aPubKeyBuf,
                  /* out */ CryptoBuffer& aXcoord,
                  /* out */ CryptoBuffer& aYcoord)
{
  pkix::Input pubKey;
  pubKey.Init(aPubKeyBuf.Elements(), aPubKeyBuf.Length());

  pkix::Reader input(pubKey);
  uint8_t b;
  if (input.Read(b) != pkix::Success) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }
  if (b != 0x04) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  nsresult rv = ReadToCryptoBuffer(input, aXcoord, 32);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = ReadToCryptoBuffer(input, aYcoord, 32);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(input.AtEnd());
  return NS_OK;
}

}
}
