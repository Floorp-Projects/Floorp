/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthentication.h"
#include "mozilla/dom/WebAuthnAssertion.h"
#include "mozilla/dom/WebAuthnAttestation.h"

#include "mozilla/dom/Promise.h"
#include "nsICryptoHash.h"
#include "pkix/Input.h"
#include "pkixutil.h"

namespace mozilla {
namespace dom {

extern mozilla::LazyLogModule gWebauthLog; // defined in U2F.cpp

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebAuthentication, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(WebAuthentication)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebAuthentication)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebAuthentication)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

template<class OOS>
static nsresult
GetAlgorithmName(JSContext* aCx, const OOS& aAlgorithm,
                 /* out */ nsString& aName)
{
  MOZ_ASSERT(aAlgorithm.IsString()); // TODO: remove assertion when we coerce.

  if (aAlgorithm.IsString()) {
    // If string, then treat as algorithm name
    aName.Assign(aAlgorithm.GetAsString());
  } else {
    // TODO: Coerce to string and extract name. See WebCryptoTask.cpp
  }

  if (!NormalizeToken(aName, aName)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  return NS_OK;
}

static nsresult
HashCString(nsICryptoHash* aHashService, const nsACString& aIn,
            /* out */ CryptoBuffer& aOut)
{
  MOZ_ASSERT(aHashService);

  nsresult rv = aHashService->Init(nsICryptoHash::SHA256);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aHashService->Update(
         reinterpret_cast<const uint8_t*>(aIn.BeginReading()),aIn.Length());
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

  aOut.Assign(fullHash);
  return rv;
}

static nsresult
AssembleClientData(const nsAString& aOrigin, const CryptoBuffer& aChallenge,
                   /* out */ nsACString& aJsonOut)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsString challengeBase64;
  nsresult rv = aChallenge.ToJwkBase64(challengeBase64);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  WebAuthnClientData clientDataObject;
  clientDataObject.mOrigin.Assign(aOrigin);
  clientDataObject.mHashAlg.SetAsString().Assign(NS_LITERAL_STRING("S256"));
  clientDataObject.mChallenge.Assign(challengeBase64);

  nsAutoString temp;
  if (NS_WARN_IF(!clientDataObject.ToJSON(temp))) {
    return NS_ERROR_FAILURE;
  }

  aJsonOut.Assign(NS_ConvertUTF16toUTF8(temp));
  return NS_OK;
}

static nsresult
ScopedCredentialGetData(const ScopedCredentialDescriptor& aSCD,
                        /* out */ uint8_t** aBuf, /* out */ uint32_t* aBufLen)
{
  MOZ_ASSERT(aBuf);
  MOZ_ASSERT(aBufLen);

  if (aSCD.mId.IsArrayBufferView()) {
    const ArrayBufferView& view = aSCD.mId.GetAsArrayBufferView();
    view.ComputeLengthAndData();
    *aBuf = view.Data();
    *aBufLen = view.Length();
  } else if (aSCD.mId.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aSCD.mId.GetAsArrayBuffer();
    buffer.ComputeLengthAndData();
    *aBuf = buffer.Data();
    *aBufLen = buffer.Length();
  } else {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static nsresult
ReadToCryptoBuffer(pkix::Reader& aSrc, /* out */ CryptoBuffer& aDest,
                   uint32_t aLen)
{
  if (aSrc.EnsureLength(aLen) != pkix::Success) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  aDest.ClearAndRetainStorage();

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

static nsresult
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

static nsresult
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
  if (pkix::der::ExpectTagAndGetValue(input, pkix::der::SEQUENCE, cert)
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

  return NS_OK;
}

WebAuthentication::WebAuthentication(nsPIDOMWindowInner* aParent)
  : mInitialized(false)
{
  mParent = do_QueryInterface(aParent);
  MOZ_ASSERT(mParent);
}

WebAuthentication::~WebAuthentication()
{}

nsresult
WebAuthentication::InitLazily()
{
  if (mInitialized) {
    return NS_OK;
  }

  MOZ_ASSERT(mParent);
  if (!mParent) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc = mParent->GetDoc();
  MOZ_ASSERT(doc);

  nsIPrincipal* principal = doc->NodePrincipal();
  nsresult rv = nsContentUtils::GetUTFOrigin(principal, mOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(mOrigin.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  // This only functions in e10s mode
  // TODO: Remove in Bug 1323339
  if (XRE_IsParentProcess()) {
    MOZ_LOG(gWebauthLog, LogLevel::Debug,
            ("Is non-e10s Process, WebAuthn not available"));
    return NS_ERROR_FAILURE;
  }

  if (Preferences::GetBool(PREF_U2F_SOFTTOKEN_ENABLED)) {
    if (!mAuthenticators.AppendElement(new NSSU2FTokenRemote(),
                                       mozilla::fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  mInitialized = true;
  return NS_OK;
}

JSObject*
WebAuthentication::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WebAuthenticationBinding::Wrap(aCx, this, aGivenProto);
}

// NOTE: This method represents a theoretical way to use a U2F-compliant token
// to produce the result of the WebAuthn MakeCredential method. The exact
// mapping of U2F data fields to WebAuthn data fields is still a matter of
// ongoing discussion, and this should not be taken as anything but a point-in-
// time possibility.
void
WebAuthentication::U2FAuthMakeCredential(
             const RefPtr<CredentialRequest>& aRequest,
             const Authenticator& aToken, CryptoBuffer& aRpIdHash,
             const nsACString& aClientData, CryptoBuffer& aClientDataHash,
             const Account& aAccount,
             const nsTArray<ScopedCredentialParameters>& aNormalizedParams,
             const Optional<Sequence<ScopedCredentialDescriptor>>& aExcludeList,
             const WebAuthnExtensions& aExtensions)
{
  MOZ_LOG(gWebauthLog, LogLevel::Debug, ("U2FAuthMakeCredential"));
  aRequest->AddActiveToken(__func__);

  // 5.1.1 When this operation is invoked, the authenticator must perform the
  // following procedure:

  // 5.1.1.a Check if all the supplied parameters are syntactically well-
  // formed and of the correct length. If not, return an error code equivalent
  // to UnknownError and terminate the operation.

  if ((aRpIdHash.Length() != SHA256_LENGTH) ||
      (aClientDataHash.Length() != SHA256_LENGTH)) {
    aRequest->SetFailure(NS_ERROR_DOM_UNKNOWN_ERR);
    return;
  }

  // 5.1.1.b Check if at least one of the specified combinations of
  // ScopedCredentialType and cryptographic parameters is supported. If not,
  // return an error code equivalent to NotSupportedError and terminate the
  // operation.

  bool isValidCombination = false;

  for (size_t a = 0; a < aNormalizedParams.Length(); ++a) {
    if (aNormalizedParams[a].mType == ScopedCredentialType::ScopedCred &&
        aNormalizedParams[a].mAlgorithm.IsString() &&
        aNormalizedParams[a].mAlgorithm.GetAsString().EqualsLiteral(
          WEBCRYPTO_NAMED_CURVE_P256)) {
      isValidCombination = true;
      break;
    }
  }
  if (!isValidCombination) {
    aRequest->SetFailure(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  // 5.1.1.c Check if a credential matching any of the supplied
  // ScopedCredential identifiers is present on this authenticator. If so,
  // return an error code equivalent to NotAllowedError and terminate the
  // operation.

  if (aExcludeList.WasPassed()) {
    const Sequence<ScopedCredentialDescriptor>& list = aExcludeList.Value();

    for (const ScopedCredentialDescriptor& scd : list) {
      bool isRegistered = false;

      uint8_t *data;
      uint32_t len;

      // data is owned by the Descriptor, do don't free it here.
      if (NS_FAILED(ScopedCredentialGetData(scd, &data, &len))) {
        aRequest->SetFailure(NS_ERROR_DOM_UNKNOWN_ERR);
        return;
      }

      nsresult rv = aToken->IsRegistered(data, len, &isRegistered);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        aRequest->SetFailure(rv);
        return;
      }

      if (isRegistered) {
        aRequest->SetFailure(NS_ERROR_DOM_NOT_ALLOWED_ERR);
        return;
      }
    }
  }

  // 5.1.1.d Prompt the user for consent to create a new credential. The
  // prompt for obtaining this consent is shown by the authenticator if it has
  // its own output capability, or by the user agent otherwise. If the user
  // denies consent, return an error code equivalent to NotAllowedError and
  // terminate the operation.

  // 5.1.1.d Once user consent has been obtained, generate a new credential
  // object

  // 5.1.1.e If any error occurred while creating the new credential object,
  // return an error code equivalent to UnknownError and terminate the
  // operation.

  // 5.1.1.f Process all the supported extensions requested by the client, and
  // generate an attestation statement. If no authority key is available to
  // sign such an attestation statement, then the authenticator performs self
  // attestation of the credential with its own private key. For more details
  // on attestation, see §5.3 Credential Attestation Statements.

  // No extensions are supported

  // 4.1.1.11 While issuedRequests is not empty, perform the following actions
  // depending upon the adjustedTimeout timer and responses from the
  // authenticators:

  // 4.1.1.11.a If the adjustedTimeout timer expires, then for each entry in
  // issuedRequests invoke the authenticatorCancel operation on that
  // authenticator and remove its entry from the list.

  uint8_t* buffer;
  uint32_t bufferlen;

  nsresult rv = aToken->Register(aRpIdHash.Elements(), aRpIdHash.Length(),
                                 aClientDataHash.Elements(),
                                 aClientDataHash.Length(), &buffer, &bufferlen);

  // 4.1.1.11.b If any authenticator returns a status indicating that the user
  // cancelled the operation, delete that authenticator’s entry from
  // issuedRequests. For each remaining entry in issuedRequests invoke the
  // authenticatorCancel operation on that authenticator and remove its entry
  // from the list.

  // 4.1.1.11.c If any authenticator returns an error status, delete the
  // corresponding entry from issuedRequests.
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRequest->SetFailure(NS_ERROR_DOM_UNKNOWN_ERR);
    return;
  }

  MOZ_ASSERT(buffer);
  CryptoBuffer regData;
  if (NS_WARN_IF(!regData.Assign(buffer, bufferlen))) {
    free(buffer);
    aRequest->SetFailure(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  free(buffer);

  // Decompose the U2F registration packet
  CryptoBuffer pubKeyBuf;
  CryptoBuffer keyHandleBuf;
  CryptoBuffer attestationCertBuf;
  CryptoBuffer signatureBuf;

  rv = U2FDecomposeRegistrationResponse(regData, pubKeyBuf, keyHandleBuf,
                                        attestationCertBuf, signatureBuf);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRequest->SetFailure(rv);
    return;
  }

  // Sign the aClientDataHash explicitly to get the format needed for
  // the AuthenticatorData parameter of WebAuthnAttestation. This might
  // be temporary while the spec settles down how to incorporate U2F.
  rv = aToken->Sign(aRpIdHash.Elements(), aRpIdHash.Length(),
                    aClientDataHash.Elements(), aClientDataHash.Length(),
                    keyHandleBuf.Elements(), keyHandleBuf.Length(), &buffer,
                    &bufferlen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRequest->SetFailure(rv);
    return;
  }

  MOZ_ASSERT(buffer);
  CryptoBuffer signatureData;
  if (NS_WARN_IF(!signatureData.Assign(buffer, bufferlen))) {
    free(buffer);
    aRequest->SetFailure(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  free(buffer);

  CryptoBuffer clientDataBuf;
  if (!clientDataBuf.Assign(aClientData)) {
    aRequest->SetFailure(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer authenticatorDataBuf;
  rv = U2FAssembleAuthenticatorData(authenticatorDataBuf, aRpIdHash,
                                    signatureData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRequest->SetFailure(rv);
    return;
  }

  // 4.1.1.11.d If any authenticator indicates success:

  // 4.1.1.11.d.1 Remove this authenticator’s entry from issuedRequests.

  // 4.1.1.11.d.2 Create a new ScopedCredentialInfo object named value and
  // populate its fields with the values returned from the authenticator as well
  // as the clientDataJSON computed earlier.

  RefPtr<ScopedCredential> credential = new ScopedCredential(this);
  credential->SetType(ScopedCredentialType::ScopedCred);
  credential->SetId(keyHandleBuf);

  RefPtr<WebAuthnAttestation> attestation = new WebAuthnAttestation(this);
  attestation->SetFormat(NS_LITERAL_STRING("u2f"));
  attestation->SetClientData(clientDataBuf);
  attestation->SetAuthenticatorData(authenticatorDataBuf);
  attestation->SetAttestation(regData);

  CredentialPtr info = new ScopedCredentialInfo(this);
  info->SetCredential(credential);
  info->SetAttestation(attestation);

  // 4.1.1.11.d.3 For each remaining entry in issuedRequests invoke the
  // authenticatorCancel operation on that authenticator and remove its entry
  // from the list.

  // 4.1.1.11.d.4 Resolve promise with value and terminate this algorithm.
  aRequest->SetSuccess(info);
}

// NOTE: This method represents a theoretical way to use a U2F-compliant token
// to produce the result of the WebAuthn GetAssertion method. The exact mapping
// of U2F data fields to WebAuthn data fields is still a matter of ongoing
// discussion, and this should not be taken as anything but a point-in- time
// possibility.
void
WebAuthentication::U2FAuthGetAssertion(const RefPtr<AssertionRequest>& aRequest,
                    const Authenticator& aToken, CryptoBuffer& aRpIdHash,
                    const nsACString& aClientData, CryptoBuffer& aClientDataHash,
                    nsTArray<CryptoBuffer>& aAllowList,
                    const WebAuthnExtensions& aExtensions)
{
  MOZ_LOG(gWebauthLog, LogLevel::Debug, ("U2FAuthGetAssertion"));

  // 4.1.2.7.e Add an entry to issuedRequests, corresponding to this request.
  aRequest->AddActiveToken(__func__);

  // 4.1.2.8 While issuedRequests is not empty, perform the following actions
  // depending upon the adjustedTimeout timer and responses from the
  // authenticators:

  // 4.1.2.8.a If the timer for adjustedTimeout expires, then for each entry
  // in issuedRequests invoke the authenticatorCancel operation on that
  // authenticator and remove its entry from the list.

  for (CryptoBuffer& allowedCredential : aAllowList) {
    bool isRegistered = false;
    nsresult rv = aToken->IsRegistered(allowedCredential.Elements(),
                                       allowedCredential.Length(),
                                       &isRegistered);

    // 4.1.2.8.b If any authenticator returns a status indicating that the user
    // cancelled the operation, delete that authenticator’s entry from
    // issuedRequests. For each remaining entry in issuedRequests invoke the
    // authenticatorCancel operation on that authenticator, and remove its entry
    // from the list.

    // 4.1.2.8.c If any authenticator returns an error status, delete the
    // corresponding entry from issuedRequests.
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRequest->SetFailure(rv);
      return;
    }

    if (!isRegistered) {
      continue;
    }

    // Sign
    uint8_t* buffer;
    uint32_t bufferlen;
    rv = aToken->Sign(aRpIdHash.Elements(), aRpIdHash.Length(),
                      aClientDataHash.Elements(), aClientDataHash.Length(),
                      allowedCredential.Elements(), allowedCredential.Length(),
                      &buffer, &bufferlen);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRequest->SetFailure(rv);
      return;
    }

    MOZ_ASSERT(buffer);
    CryptoBuffer signatureData;
    if (NS_WARN_IF(!signatureData.Assign(buffer, bufferlen))) {
      free(buffer);
      aRequest->SetFailure(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    free(buffer);

    // 4.1.2.8.d If any authenticator returns success:

    // 4.1.2.8.d.1 Remove this authenticator’s entry from issuedRequests.

    // 4.1.2.8.d.2 Create a new WebAuthnAssertion object named value and
    // populate its fields with the values returned from the authenticator as
    // well as the clientDataJSON computed earlier.

    CryptoBuffer clientDataBuf;
    if (!clientDataBuf.Assign(aClientData)) {
      aRequest->SetFailure(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    CryptoBuffer authenticatorDataBuf;
    rv = U2FAssembleAuthenticatorData(authenticatorDataBuf, aRpIdHash,
                                      signatureData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRequest->SetFailure(rv);
      return;
    }

    RefPtr<ScopedCredential> credential = new ScopedCredential(this);
    credential->SetType(ScopedCredentialType::ScopedCred);
    credential->SetId(allowedCredential);

    AssertionPtr assertion = new WebAuthnAssertion(this);
    assertion->SetCredential(credential);
    assertion->SetClientData(clientDataBuf);
    assertion->SetAuthenticatorData(authenticatorDataBuf);
    assertion->SetSignature(signatureData);

    // 4.1.2.8.d.3 For each remaining entry in issuedRequests invoke the
    // authenticatorCancel operation on that authenticator and remove its entry
    // from the list.

    // 4.1.2.8.d.4 Resolve promise with value and terminate this algorithm.
    aRequest->SetSuccess(assertion);
    return;
  }

  // 4.1.2.9 Reject promise with a DOMException whose name is "NotAllowedError",
  // and terminate this algorithm.
  aRequest->SetFailure(NS_ERROR_DOM_NOT_ALLOWED_ERR);
}

nsresult
WebAuthentication::RelaxSameOrigin(const nsAString& aInputRpId,
                                   /* out */ nsACString& aRelaxedRpId)
{
  MOZ_ASSERT(mParent);
  nsCOMPtr<nsIDocument> document = mParent->GetDoc();
  if (!document || !document->IsHTMLDocument()) {
    return NS_ERROR_FAILURE;
  }

  // TODO: Bug 1329764: Invoke the Relax Algorithm, once properly defined
  aRelaxedRpId.Assign(NS_ConvertUTF16toUTF8(aInputRpId));
  return NS_OK;
}

already_AddRefed<Promise>
WebAuthentication::MakeCredential(JSContext* aCx, const Account& aAccount,
                  const Sequence<ScopedCredentialParameters>& aCryptoParameters,
                  const ArrayBufferViewOrArrayBuffer& aChallenge,
                  const ScopedCredentialOptions& aOptions)
{
  MOZ_ASSERT(mParent);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    return nullptr;
  }

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);

  nsresult initRv = InitLazily();
  if (NS_FAILED(initRv)) {
    promise->MaybeReject(initRv);
    return promise.forget();
  }

  // 4.1.1.1 If timeoutSeconds was specified, check if its value lies within a
  // reasonable range as defined by the platform and if not, correct it to the
  // closest value lying within that range.

  double adjustedTimeout = 30.0;
  if (aOptions.mTimeoutSeconds.WasPassed()) {
    adjustedTimeout = aOptions.mTimeoutSeconds.Value();
    adjustedTimeout = std::max(15.0, adjustedTimeout);
    adjustedTimeout = std::min(120.0, adjustedTimeout);
  }

  // 4.1.1.2 Let promise be a new Promise. Return promise and start a timer for
  // adjustedTimeout seconds.

  RefPtr<CredentialRequest> requestMonitor = new CredentialRequest();
  requestMonitor->SetDeadline(TimeDuration::FromSeconds(adjustedTimeout));

  if (mOrigin.EqualsLiteral("null")) {
    // 4.1.1.3 If callerOrigin is an opaque origin, reject promise with a
    // DOMException whose name is "NotAllowedError", and terminate this
    // algorithm
    MOZ_LOG(gWebauthLog, LogLevel::Debug, ("Rejecting due to opaque origin"));
    promise->MaybeReject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return promise.forget();
  }

  nsCString rpId;
  if (!aOptions.mRpId.WasPassed()) {
    // 4.1.1.3.a If rpId is not specified, then set rpId to callerOrigin, and
    // rpIdHash to the SHA-256 hash of rpId.
    rpId.Assign(NS_ConvertUTF16toUTF8(mOrigin));
  } else {
    // 4.1.1.3.b If rpId is specified, then invoke the procedure used for
    // relaxing the same-origin restriction by setting the document.domain
    // attribute, using rpId as the given value but without changing the current
    // document’s domain. If no errors are thrown, set rpId to the value of host
    // as computed by this procedure, and rpIdHash to the SHA-256 hash of rpId.
    // Otherwise, reject promise with a DOMException whose name is
    // "SecurityError", and terminate this algorithm.

    if (NS_FAILED(RelaxSameOrigin(aOptions.mRpId.Value(), rpId))) {
      promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return promise.forget();
    }
  }

  CryptoBuffer rpIdHash;
  if (!rpIdHash.SetLength(SHA256_LENGTH, fallible)) {
    promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
    return promise.forget();
  }

  nsresult srv;
  nsCOMPtr<nsICryptoHash> hashService =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &srv);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  srv = HashCString(hashService, rpId, rpIdHash);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  // 4.1.1.4 Process each element of cryptoParameters using the following steps,
  // to produce a new sequence normalizedParameters.
  nsTArray<ScopedCredentialParameters> normalizedParams;
  for (size_t a = 0; a < aCryptoParameters.Length(); ++a) {
    // 4.1.1.4.a Let current be the currently selected element of
    // cryptoParameters.

    // 4.1.1.4.b If current.type does not contain a ScopedCredentialType
    // supported by this implementation, then stop processing current and move
    // on to the next element in cryptoParameters.
    if (aCryptoParameters[a].mType != ScopedCredentialType::ScopedCred) {
      continue;
    }

    // 4.1.1.4.c Let normalizedAlgorithm be the result of normalizing an
    // algorithm using the procedure defined in [WebCryptoAPI], with alg set to
    // current.algorithm and op set to 'generateKey'. If an error occurs during
    // this procedure, then stop processing current and move on to the next
    // element in cryptoParameters.

    nsString algName;
    if (NS_FAILED(GetAlgorithmName(aCx, aCryptoParameters[a].mAlgorithm,
                                   algName))) {
      continue;
    }

    // 4.1.1.4.d Add a new object of type ScopedCredentialParameters to
    // normalizedParameters, with type set to current.type and algorithm set to
    // normalizedAlgorithm.
    ScopedCredentialParameters normalizedObj;
    normalizedObj.mType = aCryptoParameters[a].mType;
    normalizedObj.mAlgorithm.SetAsString().Assign(algName);

    if (!normalizedParams.AppendElement(normalizedObj, mozilla::fallible)){
      promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
      return promise.forget();
    }
  }

  // 4.1.1.5 If normalizedAlgorithm is empty and cryptoParameters was not empty,
  // cancel the timer started in step 2, reject promise with a DOMException
  // whose name is "NotSupportedError", and terminate this algorithm.
  if (normalizedParams.IsEmpty() && !aCryptoParameters.IsEmpty()) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return promise.forget();
  }

  // 4.1.1.6 If excludeList is undefined, set it to the empty list.

  // 4.1.1.7 If extensions was specified, process any extensions supported by
  // this client platform, to produce the extension data that needs to be sent
  // to the authenticator. If an error is encountered while processing an
  // extension, skip that extension and do not produce any extension data for
  // it. Call the result of this processing clientExtensions.

  // Currently no extensions are supported

  // 4.1.1.8 Use attestationChallenge, callerOrigin and rpId, along with the
  // token binding key associated with callerOrigin (if any), to create a
  // ClientData structure representing this request. Choose a hash algorithm for
  // hashAlg and compute the clientDataJSON and clientDataHash.

  CryptoBuffer challenge;
  if (!challenge.Assign(aChallenge)) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  nsAutoCString clientDataJSON;
  srv = AssembleClientData(mOrigin, challenge, clientDataJSON);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  CryptoBuffer clientDataHash;
  if (!clientDataHash.SetLength(SHA256_LENGTH, fallible)) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  srv = HashCString(hashService, clientDataJSON, clientDataHash);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  // 4.1.1.9 Initialize issuedRequests to an empty list.
  RefPtr<CredentialPromise> monitorPromise = requestMonitor->Ensure();

  // 4.1.1.10 For each authenticator currently available on this platform:
  // asynchronously invoke the authenticatorMakeCredential operation on that
  // authenticator with rpIdHash, clientDataHash, accountInformation,
  // normalizedParameters, excludeList and clientExtensions as parameters. Add a
  // corresponding entry to issuedRequests.
  for (Authenticator u2ftoken : mAuthenticators) {
    // 4.1.1.10.a For each credential C in excludeList that has a non-empty
    // transports list, optionally use only the specified transports to test for
    // the existence of C.
    U2FAuthMakeCredential(requestMonitor, u2ftoken, rpIdHash, clientDataJSON,
                          clientDataHash, aAccount, normalizedParams,
                          aOptions.mExcludeList, aOptions.mExtensions);
  }

  requestMonitor->CompleteTask();

  monitorPromise->Then(AbstractThread::MainThread(), __func__,
    [promise] (CredentialPtr aInfo) {
      promise->MaybeResolve(aInfo);
    },
    [promise] (nsresult aErrorCode) {
      promise->MaybeReject(aErrorCode);
  });

  return promise.forget();
}

already_AddRefed<Promise>
WebAuthentication::GetAssertion(const ArrayBufferViewOrArrayBuffer& aChallenge,
                                const AssertionOptions& aOptions)
{
  MOZ_ASSERT(mParent);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    return nullptr;
  }

  // 4.1.2.1 If timeoutSeconds was specified, check if its value lies within a
  // reasonable range as defined by the platform and if not, correct it to the
  // closest value lying within that range.

  double adjustedTimeout = 30.0;
  if (aOptions.mTimeoutSeconds.WasPassed()) {
    adjustedTimeout = aOptions.mTimeoutSeconds.Value();
    adjustedTimeout = std::max(15.0, adjustedTimeout);
    adjustedTimeout = std::min(120.0, adjustedTimeout);
  }

  // 4.1.2.2 Let promise be a new Promise. Return promise and start a timer for
  // adjustedTimeout seconds.

  RefPtr<AssertionRequest> requestMonitor = new AssertionRequest();
  requestMonitor->SetDeadline(TimeDuration::FromSeconds(adjustedTimeout));

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);

  nsresult initRv = InitLazily();
  if (NS_FAILED(initRv)) {
    promise->MaybeReject(initRv);
    return promise.forget();
  }

  if (mOrigin.EqualsLiteral("null")) {
    // 4.1.2.3 If callerOrigin is an opaque origin, reject promise with a
    // DOMException whose name is "NotAllowedError", and terminate this algorithm
    promise->MaybeReject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return promise.forget();
  }

  nsCString rpId;
  if (!aOptions.mRpId.WasPassed()) {
    // 4.1.2.3.a If rpId is not specified, then set rpId to callerOrigin, and
    // rpIdHash to the SHA-256 hash of rpId.
    rpId.Assign(NS_ConvertUTF16toUTF8(mOrigin));
  } else {
    // 4.1.2.3.b If rpId is specified, then invoke the procedure used for
    // relaxing the same-origin restriction by setting the document.domain
    // attribute, using rpId as the given value but without changing the current
    // document’s domain. If no errors are thrown, set rpId to the value of host
    // as computed by this procedure, and rpIdHash to the SHA-256 hash of rpId.
    // Otherwise, reject promise with a DOMException whose name is
    // "SecurityError", and terminate this algorithm.

    if (NS_FAILED(RelaxSameOrigin(aOptions.mRpId.Value(), rpId))) {
      promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return promise.forget();
    }
  }

  CryptoBuffer rpIdHash;
  if (!rpIdHash.SetLength(SHA256_LENGTH, fallible)) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  nsresult srv;
  nsCOMPtr<nsICryptoHash> hashService =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &srv);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  srv = HashCString(hashService, rpId, rpIdHash);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  // 4.1.2.4 If extensions was specified, process any extensions supported by
  // this client platform, to produce the extension data that needs to be sent
  // to the authenticator. If an error is encountered while processing an
  // extension, skip that extension and do not produce any extension data for
  // it. Call the result of this processing clientExtensions.

  // TODO

  // 4.1.2.5 Use assertionChallenge, callerOrigin and rpId, along with the token
  // binding key associated with callerOrigin (if any), to create a ClientData
  // structure representing this request. Choose a hash algorithm for hashAlg
  // and compute the clientDataJSON and clientDataHash.
  CryptoBuffer challenge;
  if (!challenge.Assign(aChallenge)) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  nsAutoCString clientDataJSON;
  srv = AssembleClientData(mOrigin, challenge, clientDataJSON);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  CryptoBuffer clientDataHash;
  if (!clientDataHash.SetLength(SHA256_LENGTH, fallible)) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  srv = HashCString(hashService, clientDataJSON, clientDataHash);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  // Note: we only support U2F-style authentication for now, so we effectively
  // require an AllowList.
  if (!aOptions.mAllowList.WasPassed()) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return promise.forget();
  }

  const Sequence<ScopedCredentialDescriptor>& allowList =
    aOptions.mAllowList.Value();

  // 4.1.2.6 Initialize issuedRequests to an empty list.
  RefPtr<AssertionPromise> monitorPromise = requestMonitor->Ensure();

  // 4.1.2.7 For each authenticator currently available on this platform,
  // perform the following steps:
  for(Authenticator u2ftoken : mAuthenticators) {
    // 4.1.2.7.a If allowList is undefined or empty, let credentialList be an
    // empty list. Otherwise, execute a platform-specific procedure to determine
    // which, if any, credentials listed in allowList might be present on this
    // authenticator, and set credentialList to this filtered list. If no such
    // filtering is possible, set credentialList to an empty list.

    nsTArray<CryptoBuffer> credentialList;

    for (const ScopedCredentialDescriptor& scd : allowList) {
      CryptoBuffer buf;
      if (NS_WARN_IF(!buf.Assign(scd.mId))) {
        continue;
      }

      // 4.1.2.7.b For each credential C within the credentialList that has a
      // non- empty transports list, optionally use only the specified
      // transports to get assertions using credential C.

      // TODO: Filter using Transport
      if (!credentialList.AppendElement(buf, mozilla::fallible)) {
        requestMonitor->CancelNow();
        promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
        return promise.forget();
      }
    }

    // 4.1.2.7.c If the above filtering process concludes that none of the
    // credentials on allowList can possibly be on this authenticator, do not
    // perform any of the following steps for this authenticator, and proceed to
    // the next authenticator (if any).
    if (credentialList.IsEmpty()) {
      continue;
    }

    // 4.1.2.7.d Asynchronously invoke the authenticatorGetAssertion operation
    // on this authenticator with rpIdHash, clientDataHash, credentialList, and
    // clientExtensions as parameters.
    U2FAuthGetAssertion(requestMonitor, u2ftoken, rpIdHash, clientDataJSON,
                        clientDataHash, credentialList, aOptions.mExtensions);
  }

  requestMonitor->CompleteTask();

  monitorPromise->Then(AbstractThread::MainThread(), __func__,
    [promise] (AssertionPtr aAssertion) {
      promise->MaybeResolve(aAssertion);
    },
    [promise] (nsresult aErrorCode) {
      promise->MaybeReject(aErrorCode);
  });

  return promise.forget();
}

} // namespace dom
} // namespace mozilla
