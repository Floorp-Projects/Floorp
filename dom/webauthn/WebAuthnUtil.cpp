/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnUtil.h"
#include "nsIEffectiveTLDService.h"
#include "nsNetUtil.h"
#include "pkixutil.h"

namespace mozilla {
namespace dom {

// Bug #1436078 - Permit Google Accounts. Remove in Bug #1436085 in Jan 2023.
NS_NAMED_LITERAL_STRING(kGoogleAccountsAppId1,
  "https://www.gstatic.com/securitykey/origins.json");
NS_NAMED_LITERAL_STRING(kGoogleAccountsAppId2,
  "https://www.gstatic.com/securitykey/a/google.com/origins.json");

const uint8_t FLAG_TUP = 0x01; // Test of User Presence required
const uint8_t FLAG_AT = 0x40; // Authenticator Data is provided

bool
EvaluateAppID(nsPIDOMWindowInner* aParent, const nsString& aOrigin,
              const U2FOperation& aOp, /* in/out */ nsString& aAppId)
{
  // Facet is the specification's way of referring to the web origin.
  nsAutoCString facetString = NS_ConvertUTF16toUTF8(aOrigin);
  nsCOMPtr<nsIURI> facetUri;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(facetUri), facetString))) {
    return false;
  }

  // If the facetId (origin) is not HTTPS, reject
  bool facetIsHttps = false;
  if (NS_FAILED(facetUri->SchemeIs("https", &facetIsHttps)) || !facetIsHttps) {
    return false;
  }

  // If the appId is empty or null, overwrite it with the facetId and accept
  if (aAppId.IsEmpty() || aAppId.EqualsLiteral("null")) {
    aAppId.Assign(aOrigin);
    return true;
  }

  // AppID is user-supplied. It's quite possible for this parse to fail.
  nsAutoCString appIdString = NS_ConvertUTF16toUTF8(aAppId);
  nsCOMPtr<nsIURI> appIdUri;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(appIdUri), appIdString))) {
    return false;
  }

  // if the appId URL is not HTTPS, reject.
  bool appIdIsHttps = false;
  if (NS_FAILED(appIdUri->SchemeIs("https", &appIdIsHttps)) || !appIdIsHttps) {
    return false;
  }

  nsAutoCString appIdHost;
  if (NS_FAILED(appIdUri->GetAsciiHost(appIdHost))) {
    return false;
  }

  // Allow localhost.
  if (appIdHost.EqualsLiteral("localhost")) {
    nsAutoCString facetHost;
    if (NS_FAILED(facetUri->GetAsciiHost(facetHost))) {
      return false;
    }

    if (facetHost.EqualsLiteral("localhost")) {
      return true;
    }
  }

  // Run the HTML5 algorithm to relax the same-origin policy, copied from W3C
  // Web Authentication. See Bug 1244959 comment #8 for context on why we are
  // doing this instead of implementing the external-fetch FacetID logic.
  nsCOMPtr<nsIDocument> document = aParent->GetDoc();
  if (!document || !document->IsHTMLDocument()) {
    return false;
  }

  nsHTMLDocument* html = document->AsHTMLDocument();
  // Use the base domain as the facet for evaluation. This lets this algorithm
  // relax the whole eTLD+1.
  nsCOMPtr<nsIEffectiveTLDService> tldService =
    do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (!tldService) {
    return false;
  }

  nsAutoCString lowestFacetHost;
  if (NS_FAILED(tldService->GetBaseDomain(facetUri, 0, lowestFacetHost))) {
    return false;
  }

  if (html->IsRegistrableDomainSuffixOfOrEqualTo(NS_ConvertUTF8toUTF16(lowestFacetHost),
                                                 appIdHost)) {
    return true;
  }

  // Bug #1436078 - Permit Google Accounts. Remove in Bug #1436085 in Jan 2023.
  if (aOp == U2FOperation::Sign && lowestFacetHost.EqualsLiteral("google.com") &&
      (aAppId.Equals(kGoogleAccountsAppId1) ||
       aAppId.Equals(kGoogleAccountsAppId2))) {
    return true;
  }

  return false;
}


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

  authDataBuf.AppendElements(rpIdHashBuf, mozilla::fallible);
  authDataBuf.AppendElement(flags, mozilla::fallible);
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
AssembleAttestationObject(const CryptoBuffer& aRpIdHash,
                          const CryptoBuffer& aPubKeyBuf,
                          const CryptoBuffer& aKeyHandleBuf,
                          const CryptoBuffer& aAttestationCertBuf,
                          const CryptoBuffer& aSignatureBuf,
                          bool aForceNoneAttestation,
                          /* out */ CryptoBuffer& aAttestationObjBuf)
{
  // Construct the public key object
  CryptoBuffer pubKeyObj;
  nsresult rv = CBOREncodePublicKeyObj(aPubKeyBuf, pubKeyObj);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mozilla::dom::CryptoBuffer aaguidBuf;
  if (NS_WARN_IF(!aaguidBuf.SetCapacity(16, mozilla::fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // FIDO U2F devices have no AAGUIDs, so they'll be all zeros until we add
  // support for CTAP2 devices.
  for (int i=0; i<16; i++) {
    aaguidBuf.AppendElement(0x00, mozilla::fallible);
  }

  // During create credential, counter is always 0 for U2F
  // See https://github.com/w3c/webauthn/issues/507
  mozilla::dom::CryptoBuffer counterBuf;
  if (NS_WARN_IF(!counterBuf.SetCapacity(4, mozilla::fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  counterBuf.AppendElement(0x00, mozilla::fallible);
  counterBuf.AppendElement(0x00, mozilla::fallible);
  counterBuf.AppendElement(0x00, mozilla::fallible);
  counterBuf.AppendElement(0x00, mozilla::fallible);

  // Construct the Attestation Data, which slots into the end of the
  // Authentication Data buffer.
  CryptoBuffer attDataBuf;
  rv = AssembleAttestationData(aaguidBuf, aKeyHandleBuf, pubKeyObj, attDataBuf);
  if (NS_FAILED(rv)) {
    return rv;
  }

  CryptoBuffer authDataBuf;
  // attDataBuf always contains data, so per [1] we have to set the AT flag.
  // [1] https://w3c.github.io/webauthn/#sec-authenticator-data
  const uint8_t flags = FLAG_TUP | FLAG_AT;
  rv = AssembleAuthenticatorData(aRpIdHash, flags, counterBuf, attDataBuf,
                                 authDataBuf);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Direct attestation might have been requested by the RP.
  // The user might override this and request anonymization anyway.
  if (aForceNoneAttestation) {
    rv = CBOREncodeNoneAttestationObj(authDataBuf, aAttestationObjBuf);
  } else {
    rv = CBOREncodeFidoU2FAttestationObj(authDataBuf, aAttestationCertBuf,
                                         aSignatureBuf, aAttestationObjBuf);
  }

  return rv;
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

static nsresult
HashCString(nsICryptoHash* aHashService,
            const nsACString& aIn,
            /* out */ CryptoBuffer& aOut)
{
  MOZ_ASSERT(aHashService);

  nsresult rv = aHashService->Init(nsICryptoHash::SHA256);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aHashService->Update(
    reinterpret_cast<const uint8_t*>(aIn.BeginReading()), aIn.Length());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString fullHash;
  // Passing false below means we will get a binary result rather than a
  // base64-encoded string.
  rv = aHashService->Finish(false, fullHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!aOut.Assign(fullHash))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
HashCString(const nsACString& aIn, /* out */ CryptoBuffer& aOut)
{
  nsresult srv;
  nsCOMPtr<nsICryptoHash> hashService =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &srv);
  if (NS_FAILED(srv)) {
    return srv;
  }

  srv = HashCString(hashService, aIn, aOut);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
BuildTransactionHashes(const nsCString& aRpId,
                       const nsCString& aClientDataJSON,
                       /* out */ CryptoBuffer& aRpIdHash,
                       /* out */ CryptoBuffer& aClientDataHash)
{
  nsresult srv;
  nsCOMPtr<nsICryptoHash> hashService =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &srv);
  if (NS_FAILED(srv)) {
    return srv;
  }

  if (!aRpIdHash.SetLength(SHA256_LENGTH, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  srv = HashCString(hashService, aRpId, aRpIdHash);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    return NS_ERROR_FAILURE;
  }

  if (!aClientDataHash.SetLength(SHA256_LENGTH, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  srv = HashCString(hashService, aClientDataJSON, aClientDataHash);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


}
}
