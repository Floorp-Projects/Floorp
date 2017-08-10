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

// Format:
// 32 bytes: SHA256 of the RP ID
// 1 byte: flags (TUP & AT)
// 4 bytes: sign counter
// variable: attestation data struct
// variable: CBOR-format extension auth data (optional, not flagged)
nsresult
AssembleAuthenticatorData(const CryptoBuffer& rpIdHashBuf,
                          const uint8_t flags,
                          const CryptoBuffer& counterBuf,
                          const CryptoBuffer& attestationDataBuf,
                          /* out */ CryptoBuffer& authDataBuf)
{
  if (NS_WARN_IF(!authDataBuf.SetCapacity(32 + 1 + 4 + attestationDataBuf.Length(),
                                          mozilla::fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (rpIdHashBuf.Length() != 32 || counterBuf.Length() != 4) {
    return NS_ERROR_INVALID_ARG;
  }

  uint8_t flagSet = flags;
  if (!attestationDataBuf.IsEmpty()) {
    flagSet |= FLAG_AT;
  }

  authDataBuf.AppendElements(rpIdHashBuf, mozilla::fallible);
  authDataBuf.AppendElement(flagSet, mozilla::fallible);
  authDataBuf.AppendElements(counterBuf, mozilla::fallible);
  authDataBuf.AppendElements(attestationDataBuf, mozilla::fallible);
  return NS_OK;
}

// attestation data struct format:
// - 16 bytes: AAGUID
// - 2 bytes: Length of Credential ID
// - L bytes: Credential ID
// - variable: CBOR-format public key
nsresult
AssembleAttestationData(const CryptoBuffer& aaguidBuf,
                        const CryptoBuffer& keyHandleBuf,
                        const CryptoBuffer& pubKeyObj,
                        /* out */ CryptoBuffer& attestationDataBuf)
{
  if (NS_WARN_IF(!attestationDataBuf.SetCapacity(aaguidBuf.Length() + 2 +
                                                 keyHandleBuf.Length() +
                                                 pubKeyObj.Length(),
                                                 mozilla::fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (keyHandleBuf.Length() > 0xFFFF) {
    return NS_ERROR_INVALID_ARG;
  }

  attestationDataBuf.AppendElements(aaguidBuf, mozilla::fallible);
  attestationDataBuf.AppendElement((keyHandleBuf.Length() >> 8) & 0xFF,
                                   mozilla::fallible);
  attestationDataBuf.AppendElement((keyHandleBuf.Length() >> 0) & 0xFF,
                                   mozilla::fallible);
  attestationDataBuf.AppendElements(keyHandleBuf, mozilla::fallible);
  attestationDataBuf.AppendElements(pubKeyObj, mozilla::fallible);
  return NS_OK;
}

nsresult
U2FDecomposeSignResponse(const CryptoBuffer& aResponse,
                         /* out */ uint8_t& aFlags,
                         /* out */ CryptoBuffer& aCounterBuf,
                         /* out */ CryptoBuffer& aSignatureBuf)
{
  if (aResponse.Length() < 5) {
    return NS_ERROR_INVALID_ARG;
  }

  Span<const uint8_t> rspView = MakeSpan(aResponse);
  aFlags = rspView[0];

  if (NS_WARN_IF(!aCounterBuf.AppendElements(rspView.FromTo(1, 5),
                                             mozilla::fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_WARN_IF(!aSignatureBuf.AppendElements(rspView.From(5),
                                               mozilla::fallible))) {
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
