/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "hasht.h"
#include "nsHTMLDocument.h"
#include "nsICryptoHash.h"
#include "nsNetCID.h"
#include "nsThreadUtils.h"
#include "WebAuthnCoseIdentifiers.h"
#include "mozilla/dom/AuthenticatorAttestationResponse.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PWebAuthnTransaction.h"
#include "mozilla/dom/U2FUtil.h"
#include "mozilla/dom/WebAuthnCBORUtil.h"
#include "mozilla/dom/WebAuthnManager.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/dom/WebAuthnUtil.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

/***********************************************************************
 * Protocol Constants
 **********************************************************************/

const uint8_t FLAG_TUP = 0x01; // Test of User Presence required
const uint8_t FLAG_AT = 0x40; // Authenticator Data is provided
const uint8_t FLAG_UV = 0x04; // User was Verified (biometrics, etc.); this
                              // flag is not possible with U2F devices

/***********************************************************************
 * Statics
 **********************************************************************/

namespace {
static mozilla::LazyLogModule gWebAuthnManagerLog("webauthnmanager");
}

NS_IMPL_ISUPPORTS(WebAuthnManager, nsIDOMEventListener);

/***********************************************************************
 * Utility Functions
 **********************************************************************/

static nsresult
AssembleClientData(const nsAString& aOrigin,
                   const CryptoBuffer& aChallenge,
                   const nsAString& aType,
                   const AuthenticationExtensionsClientInputs& aExtensions,
                   /* out */ nsACString& aJsonOut)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsString challengeBase64;
  nsresult rv = aChallenge.ToJwkBase64(challengeBase64);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  CollectedClientData clientDataObject;
  clientDataObject.mType.Assign(aType);
  clientDataObject.mChallenge.Assign(challengeBase64);
  clientDataObject.mOrigin.Assign(aOrigin);
  clientDataObject.mHashAlgorithm.AssignLiteral(u"SHA-256");
  clientDataObject.mClientExtensions = aExtensions;

  nsAutoString temp;
  if (NS_WARN_IF(!clientDataObject.ToJSON(temp))) {
    return NS_ERROR_FAILURE;
  }

  aJsonOut.Assign(NS_ConvertUTF16toUTF8(temp));
  return NS_OK;
}

nsresult
GetOrigin(nsPIDOMWindowInner* aParent,
          /*out*/ nsAString& aOrigin, /*out*/ nsACString& aHost)
{
  MOZ_ASSERT(aParent);
  nsCOMPtr<nsIDocument> doc = aParent->GetDoc();
  MOZ_ASSERT(doc);

  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  nsresult rv = nsContentUtils::GetUTFOrigin(principal, aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv)) ||
      NS_WARN_IF(aOrigin.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  if (aOrigin.EqualsLiteral("null")) {
    // 4.1.1.3 If callerOrigin is an opaque origin, reject promise with a
    // DOMException whose name is "NotAllowedError", and terminate this
    // algorithm
    MOZ_LOG(gWebAuthnManagerLog, LogLevel::Debug, ("Rejecting due to opaque origin"));
    return NS_ERROR_DOM_NOT_ALLOWED_ERR;
  }

  nsCOMPtr<nsIURI> originUri;
  if (NS_FAILED(principal->GetURI(getter_AddRefs(originUri)))) {
    return NS_ERROR_FAILURE;
  }
  if (NS_FAILED(originUri->GetAsciiHost(aHost))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
RelaxSameOrigin(nsPIDOMWindowInner* aParent,
                const nsAString& aInputRpId,
                /* out */ nsACString& aRelaxedRpId)
{
  MOZ_ASSERT(aParent);
  nsCOMPtr<nsIDocument> doc = aParent->GetDoc();
  MOZ_ASSERT(doc);

  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(principal->GetURI(getter_AddRefs(uri)))) {
    return NS_ERROR_FAILURE;
  }
  nsAutoCString originHost;
  if (NS_FAILED(uri->GetAsciiHost(originHost))) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDocument> document = aParent->GetDoc();
  if (!document || !document->IsHTMLDocument()) {
    return NS_ERROR_FAILURE;
  }
  nsHTMLDocument* html = document->AsHTMLDocument();
  if (!html->IsRegistrableDomainSuffixOfOrEqualTo(aInputRpId, originHost)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  aRelaxedRpId.Assign(NS_ConvertUTF16toUTF8(aInputRpId));
  return NS_OK;
}

/***********************************************************************
 * WebAuthnManager Implementation
 **********************************************************************/

void
WebAuthnManager::ClearTransaction()
{
  if (!NS_WARN_IF(mTransaction.isNothing())) {
    StopListeningForVisibilityEvents();
  }

  mTransaction.reset();
  Unfollow();
}

void
WebAuthnManager::RejectTransaction(const nsresult& aError)
{
  if (!NS_WARN_IF(mTransaction.isNothing())) {
    mTransaction.ref().mPromise->MaybeReject(aError);
  }

  ClearTransaction();
}

void
WebAuthnManager::CancelTransaction(const nsresult& aError)
{
  if (!NS_WARN_IF(!mChild || mTransaction.isNothing())) {
    mChild->SendRequestCancel(mTransaction.ref().mId);
  }

  RejectTransaction(aError);
}

WebAuthnManager::~WebAuthnManager()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome()) {
    RejectTransaction(NS_ERROR_ABORT);
  }

  if (mChild) {
    RefPtr<WebAuthnTransactionChild> c;
    mChild.swap(c);
    c->Disconnect();
  }
}

already_AddRefed<Promise>
WebAuthnManager::MakeCredential(const PublicKeyCredentialCreationOptions& aOptions,
                                const Optional<OwningNonNull<AbortSignal>>& aSignal)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome()) {
    CancelTransaction(NS_ERROR_ABORT);
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  // Abort the request if aborted flag is already set.
  if (aSignal.WasPassed() && aSignal.Value().Aborted()) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return promise.forget();
  }

  nsString origin;
  nsCString rpId;
  rv = GetOrigin(mParent, origin, rpId);
  if (NS_WARN_IF(rv.Failed())) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  // Enforce 5.4.3 User Account Parameters for Credential Generation
  // When we add UX, we'll want to do more with this value, but for now
  // we just have to verify its correctness.
  {
    CryptoBuffer userId;
    userId.Assign(aOptions.mUser.mId);
    if (userId.Length() > 64) {
      promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR);
      return promise.forget();
    }
  }

  // If timeoutSeconds was specified, check if its value lies within a
  // reasonable range as defined by the platform and if not, correct it to the
  // closest value lying within that range.

  uint32_t adjustedTimeout = 30000;
  if (aOptions.mTimeout.WasPassed()) {
    adjustedTimeout = aOptions.mTimeout.Value();
    adjustedTimeout = std::max(15000u, adjustedTimeout);
    adjustedTimeout = std::min(120000u, adjustedTimeout);
  }

  if (aOptions.mRp.mId.WasPassed()) {
    // If rpId is specified, then invoke the procedure used for relaxing the
    // same-origin restriction by setting the document.domain attribute, using
    // rpId as the given value but without changing the current document’s
    // domain. If no errors are thrown, set rpId to the value of host as
    // computed by this procedure, and rpIdHash to the SHA-256 hash of rpId.
    // Otherwise, reject promise with a DOMException whose name is
    // "SecurityError", and terminate this algorithm.

    if (NS_FAILED(RelaxSameOrigin(mParent, aOptions.mRp.mId.Value(), rpId))) {
      promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return promise.forget();
    }
  }

  // <https://w3c.github.io/webauthn/#sctn-appid-extension>
  if (aOptions.mExtensions.mAppid.WasPassed()) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return promise.forget();
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


  // TODO: Move this logic into U2FTokenManager in Bug 1409220.

  // Process each element of mPubKeyCredParams using the following steps, to
  // produce a new sequence acceptableParams.
  nsTArray<PublicKeyCredentialParameters> acceptableParams;
  for (size_t a = 0; a < aOptions.mPubKeyCredParams.Length(); ++a) {
    // Let current be the currently selected element of
    // mPubKeyCredParams.

    // If current.type does not contain a PublicKeyCredentialType
    // supported by this implementation, then stop processing current and move
    // on to the next element in mPubKeyCredParams.
    if (aOptions.mPubKeyCredParams[a].mType != PublicKeyCredentialType::Public_key) {
      continue;
    }

    nsString algName;
    if (NS_FAILED(CoseAlgorithmToWebCryptoId(aOptions.mPubKeyCredParams[a].mAlg,
                                             algName))) {
      continue;
    }

    if (!acceptableParams.AppendElement(aOptions.mPubKeyCredParams[a],
                                        mozilla::fallible)){
      promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
      return promise.forget();
    }
  }

  // If acceptableParams is empty and mPubKeyCredParams was not empty, cancel
  // the timer started in step 2, reject promise with a DOMException whose name
  // is "NotSupportedError", and terminate this algorithm.
  if (acceptableParams.IsEmpty() && !aOptions.mPubKeyCredParams.IsEmpty()) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return promise.forget();
  }

  // If excludeList is undefined, set it to the empty list.
  //
  // If extensions was specified, process any extensions supported by this
  // client platform, to produce the extension data that needs to be sent to the
  // authenticator. If an error is encountered while processing an extension,
  // skip that extension and do not produce any extension data for it. Call the
  // result of this processing clientExtensions.
  //
  // Currently no extensions are supported
  //
  // Use attestationChallenge, callerOrigin and rpId, along with the token
  // binding key associated with callerOrigin (if any), to create a ClientData
  // structure representing this request. Choose a hash algorithm for hashAlg
  // and compute the clientDataJSON and clientDataHash.

  CryptoBuffer challenge;
  if (!challenge.Assign(aOptions.mChallenge)) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  nsAutoCString clientDataJSON;
  srv = AssembleClientData(origin, challenge,
                           NS_LITERAL_STRING("webauthn.create"),
                           aOptions.mExtensions, clientDataJSON);
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

  nsTArray<WebAuthnScopedCredential> excludeList;
  for (const auto& s: aOptions.mExcludeCredentials) {
    WebAuthnScopedCredential c;
    CryptoBuffer cb;
    cb.Assign(s.mId);
    c.id() = cb;
    excludeList.AppendElement(c);
  }

  if (!MaybeCreateBackgroundActor()) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return promise.forget();
  }

  // TODO: Add extension list building
  nsTArray<WebAuthnExtension> extensions;

  const auto& selection = aOptions.mAuthenticatorSelection;
  const auto& attachment = selection.mAuthenticatorAttachment;
  const AttestationConveyancePreference& attestation = aOptions.mAttestation;

  // Does the RP require attachment == "platform"?
  bool requirePlatformAttachment =
    attachment.WasPassed() && attachment.Value() == AuthenticatorAttachment::Platform;

  // Does the RP require user verification?
  bool requireUserVerification =
    selection.mUserVerification == UserVerificationRequirement::Required;

  // Does the RP desire direct attestation? Indirect attestation is not
  // implemented, and thus is equivilent to None.
  bool requestDirectAttestation =
    attestation == AttestationConveyancePreference::Direct;

  // Create and forward authenticator selection criteria.
  WebAuthnAuthenticatorSelection authSelection(selection.mRequireResidentKey,
                                               requireUserVerification,
                                               requirePlatformAttachment);

  WebAuthnMakeCredentialInfo info(origin,
                                  rpIdHash,
                                  clientDataHash,
                                  adjustedTimeout,
                                  excludeList,
                                  extensions,
                                  authSelection,
                                  requestDirectAttestation);

  ListenForVisibilityEvents();

  AbortSignal* signal = nullptr;
  if (aSignal.WasPassed()) {
    signal = &aSignal.Value();
    Follow(signal);
  }

  MOZ_ASSERT(mTransaction.isNothing());
  mTransaction = Some(WebAuthnTransaction(promise,
                                          rpIdHash,
                                          clientDataJSON,
                                          requestDirectAttestation,
                                          signal));

  mChild->SendRequestRegister(mTransaction.ref().mId, info);

  return promise.forget();
}

already_AddRefed<Promise>
WebAuthnManager::GetAssertion(const PublicKeyCredentialRequestOptions& aOptions,
                              const Optional<OwningNonNull<AbortSignal>>& aSignal)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome()) {
    CancelTransaction(NS_ERROR_ABORT);
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  // Abort the request if aborted flag is already set.
  if (aSignal.WasPassed() && aSignal.Value().Aborted()) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return promise.forget();
  }

  nsString origin;
  nsCString rpId;
  rv = GetOrigin(mParent, origin, rpId);
  if (NS_WARN_IF(rv.Failed())) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  // If timeoutSeconds was specified, check if its value lies within a
  // reasonable range as defined by the platform and if not, correct it to the
  // closest value lying within that range.

  uint32_t adjustedTimeout = 30000;
  if (aOptions.mTimeout.WasPassed()) {
    adjustedTimeout = aOptions.mTimeout.Value();
    adjustedTimeout = std::max(15000u, adjustedTimeout);
    adjustedTimeout = std::min(120000u, adjustedTimeout);
  }

  if (aOptions.mRpId.WasPassed()) {
    // If rpId is specified, then invoke the procedure used for relaxing the
    // same-origin restriction by setting the document.domain attribute, using
    // rpId as the given value but without changing the current document’s
    // domain. If no errors are thrown, set rpId to the value of host as
    // computed by this procedure, and rpIdHash to the SHA-256 hash of rpId.
    // Otherwise, reject promise with a DOMException whose name is
    // "SecurityError", and terminate this algorithm.

    if (NS_FAILED(RelaxSameOrigin(mParent, aOptions.mRpId.Value(), rpId))) {
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

  // Use assertionChallenge, callerOrigin and rpId, along with the token binding
  // key associated with callerOrigin (if any), to create a ClientData structure
  // representing this request. Choose a hash algorithm for hashAlg and compute
  // the clientDataJSON and clientDataHash.
  CryptoBuffer challenge;
  if (!challenge.Assign(aOptions.mChallenge)) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  nsAutoCString clientDataJSON;
  srv = AssembleClientData(origin, challenge, NS_LITERAL_STRING("webauthn.get"),
                           aOptions.mExtensions, clientDataJSON);
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

  nsTArray<WebAuthnScopedCredential> allowList;
  for (const auto& s: aOptions.mAllowCredentials) {
    if (s.mType == PublicKeyCredentialType::Public_key) {
      WebAuthnScopedCredential c;
      CryptoBuffer cb;
      cb.Assign(s.mId);
      c.id() = cb;

      // Serialize transports.
      if (s.mTransports.WasPassed()) {
        uint8_t transports = 0;
        for (const auto& t: s.mTransports.Value()) {
          if (t == AuthenticatorTransport::Usb) {
            transports |= U2F_AUTHENTICATOR_TRANSPORT_USB;
          }
          if (t == AuthenticatorTransport::Nfc) {
            transports |= U2F_AUTHENTICATOR_TRANSPORT_NFC;
          }
          if (t == AuthenticatorTransport::Ble) {
            transports |= U2F_AUTHENTICATOR_TRANSPORT_BLE;
          }
        }
        c.transports() = transports;
      }

      allowList.AppendElement(c);
    }
  }

  if (!MaybeCreateBackgroundActor()) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return promise.forget();
  }

  // Does the RP require user verification?
  bool requireUserVerification =
    aOptions.mUserVerification == UserVerificationRequirement::Required;

  // If extensions were specified, process any extensions supported by this
  // client platform, to produce the extension data that needs to be sent to the
  // authenticator. If an error is encountered while processing an extension,
  // skip that extension and do not produce any extension data for it. Call the
  // result of this processing clientExtensions.
  nsTArray<WebAuthnExtension> extensions;

  // <https://w3c.github.io/webauthn/#sctn-appid-extension>
  if (aOptions.mExtensions.mAppid.WasPassed()) {
    nsString appId(aOptions.mExtensions.mAppid.Value());

    // Check that the appId value is allowed.
    if (!EvaluateAppID(mParent, origin, U2FOperation::Sign, appId)) {
      promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return promise.forget();
    }

    CryptoBuffer appIdHash;
    if (!appIdHash.SetLength(SHA256_LENGTH, fallible)) {
      promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
      return promise.forget();
    }

    // We need the SHA-256 hash of the appId.
    nsresult srv = HashCString(hashService, NS_ConvertUTF16toUTF8(appId), appIdHash);
    if (NS_WARN_IF(NS_FAILED(srv))) {
      promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return promise.forget();
    }

    // Append the hash and send it to the backend.
    extensions.AppendElement(WebAuthnExtensionAppId(appIdHash));
  }

  WebAuthnGetAssertionInfo info(origin,
                                rpIdHash,
                                clientDataHash,
                                adjustedTimeout,
                                allowList,
                                requireUserVerification,
                                extensions);

  ListenForVisibilityEvents();

  AbortSignal* signal = nullptr;
  if (aSignal.WasPassed()) {
    signal = &aSignal.Value();
    Follow(signal);
  }

  MOZ_ASSERT(mTransaction.isNothing());
  mTransaction = Some(WebAuthnTransaction(promise,
                                          rpIdHash,
                                          clientDataJSON,
                                          false /* aDirectAttestation */,
                                          signal));

  mChild->SendRequestSign(mTransaction.ref().mId, info);
  return promise.forget();
}

already_AddRefed<Promise>
WebAuthnManager::Store(const Credential& aCredential)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome()) {
    CancelTransaction(NS_ERROR_ABORT);
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return promise.forget();
}

void
WebAuthnManager::FinishMakeCredential(const uint64_t& aTransactionId,
                                      const WebAuthnMakeCredentialResult& aResult)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Check for a valid transaction.
  if (mTransaction.isNothing() || mTransaction.ref().mId != aTransactionId) {
    return;
  }

  CryptoBuffer regData;
  if (NS_WARN_IF(!regData.Assign(aResult.RegBuffer().Elements(),
                                 aResult.RegBuffer().Length()))) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  mozilla::dom::CryptoBuffer aaguidBuf;
  if (NS_WARN_IF(!aaguidBuf.SetCapacity(16, mozilla::fallible))) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  // FIDO U2F devices have no AAGUIDs, so they'll be all zeros until we add
  // support for CTAP2 devices.
  for (int i=0; i<16; i++) {
    aaguidBuf.AppendElement(0x00, mozilla::fallible);
  }

  // Decompose the U2F registration packet
  CryptoBuffer pubKeyBuf;
  CryptoBuffer keyHandleBuf;
  CryptoBuffer attestationCertBuf;
  CryptoBuffer signatureBuf;

  // Only handles attestation cert chains of length=1.
  nsresult rv = U2FDecomposeRegistrationResponse(regData, pubKeyBuf, keyHandleBuf,
                                                 attestationCertBuf, signatureBuf);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectTransaction(rv);
    return;
  }
  MOZ_ASSERT(keyHandleBuf.Length() <= 0xFFFF);

  nsAutoString keyHandleBase64Url;
  rv = keyHandleBuf.ToJwkBase64(keyHandleBase64Url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectTransaction(rv);
    return;
  }

  CryptoBuffer clientDataBuf;
  if (!clientDataBuf.Assign(mTransaction.ref().mClientData)) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer rpIdHashBuf;
  if (!rpIdHashBuf.Assign(mTransaction.ref().mRpIdHash)) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  // Construct the public key object
  CryptoBuffer pubKeyObj;
  rv = CBOREncodePublicKeyObj(pubKeyBuf, pubKeyObj);
  if (NS_FAILED(rv)) {
    RejectTransaction(rv);
    return;
  }

  // During create credential, counter is always 0 for U2F
  // See https://github.com/w3c/webauthn/issues/507
  mozilla::dom::CryptoBuffer counterBuf;
  if (NS_WARN_IF(!counterBuf.SetCapacity(4, mozilla::fallible))) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  counterBuf.AppendElement(0x00, mozilla::fallible);
  counterBuf.AppendElement(0x00, mozilla::fallible);
  counterBuf.AppendElement(0x00, mozilla::fallible);
  counterBuf.AppendElement(0x00, mozilla::fallible);

  // Construct the Attestation Data, which slots into the end of the
  // Authentication Data buffer.
  CryptoBuffer attDataBuf;
  rv = AssembleAttestationData(aaguidBuf, keyHandleBuf, pubKeyObj, attDataBuf);
  if (NS_FAILED(rv)) {
    RejectTransaction(rv);
    return;
  }

  mozilla::dom::CryptoBuffer authDataBuf;
  // attDataBuf always contains data, so per [1] we have to set the AT flag.
  // [1] https://w3c.github.io/webauthn/#sec-authenticator-data
  const uint8_t flags = FLAG_TUP | FLAG_AT;
  rv = AssembleAuthenticatorData(rpIdHashBuf, flags, counterBuf, attDataBuf,
                                 authDataBuf);
  if (NS_FAILED(rv)) {
    RejectTransaction(rv);
    return;
  }

  // Direct attestation might have been requested by the RP. This will
  // be true only if the user consented via the permission UI.
  CryptoBuffer attObj;
  if (aResult.DirectAttestationPermitted()) {
    rv = CBOREncodeFidoU2FAttestationObj(authDataBuf, attestationCertBuf,
                                         signatureBuf, attObj);
  } else {
    rv = CBOREncodeNoneAttestationObj(authDataBuf, attObj);
  }

  if (NS_FAILED(rv)) {
    RejectTransaction(rv);
    return;
  }

  // Create a new PublicKeyCredential object and populate its fields with the
  // values returned from the authenticator as well as the clientDataJSON
  // computed earlier.
  RefPtr<AuthenticatorAttestationResponse> attestation =
      new AuthenticatorAttestationResponse(mParent);
  attestation->SetClientDataJSON(clientDataBuf);
  attestation->SetAttestationObject(attObj);

  RefPtr<PublicKeyCredential> credential =
      new PublicKeyCredential(mParent);
  credential->SetId(keyHandleBase64Url);
  credential->SetType(NS_LITERAL_STRING("public-key"));
  credential->SetRawId(keyHandleBuf);
  credential->SetResponse(attestation);

  mTransaction.ref().mPromise->MaybeResolve(credential);
  ClearTransaction();
}

void
WebAuthnManager::FinishGetAssertion(const uint64_t& aTransactionId,
                                    const WebAuthnGetAssertionResult& aResult)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Check for a valid transaction.
  if (mTransaction.isNothing() || mTransaction.ref().mId != aTransactionId) {
    return;
  }

  CryptoBuffer tokenSignatureData;
  if (NS_WARN_IF(!tokenSignatureData.Assign(aResult.SigBuffer().Elements(),
                                            aResult.SigBuffer().Length()))) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer clientDataBuf;
  if (!clientDataBuf.Assign(mTransaction.ref().mClientData)) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer rpIdHashBuf;
  if (!rpIdHashBuf.Assign(aResult.RpIdHash())) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer signatureBuf;
  CryptoBuffer counterBuf;
  uint8_t flags = 0;
  nsresult rv = U2FDecomposeSignResponse(tokenSignatureData, flags, counterBuf,
                                         signatureBuf);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectTransaction(rv);
    return;
  }

  // Preserve the two LSBs of the flags byte, UP and RFU1.
  // See <https://github.com/fido-alliance/fido-2-specs/pull/519>
  flags &= 0b11;

  CryptoBuffer attestationDataBuf;
  CryptoBuffer authenticatorDataBuf;
  rv = AssembleAuthenticatorData(rpIdHashBuf, flags, counterBuf,
                                 /* deliberately empty */ attestationDataBuf,
                                 authenticatorDataBuf);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectTransaction(rv);
    return;
  }

  CryptoBuffer credentialBuf;
  if (!credentialBuf.Assign(aResult.CredentialID())) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsAutoString credentialBase64Url;
  rv = credentialBuf.ToJwkBase64(credentialBase64Url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectTransaction(rv);
    return;
  }

  // If any authenticator returns success:

  // Create a new PublicKeyCredential object named value and populate its fields
  // with the values returned from the authenticator as well as the
  // clientDataJSON computed earlier.
  RefPtr<AuthenticatorAssertionResponse> assertion =
    new AuthenticatorAssertionResponse(mParent);
  assertion->SetClientDataJSON(clientDataBuf);
  assertion->SetAuthenticatorData(authenticatorDataBuf);
  assertion->SetSignature(signatureBuf);

  RefPtr<PublicKeyCredential> credential = new PublicKeyCredential(mParent);
  credential->SetId(credentialBase64Url);
  credential->SetType(NS_LITERAL_STRING("public-key"));
  credential->SetRawId(credentialBuf);
  credential->SetResponse(assertion);

  // Forward client extension results.
  for (auto& ext: aResult.Extensions()) {
    if (ext.type() == WebAuthnExtensionResult::TWebAuthnExtensionResultAppId) {
      bool appid = ext.get_WebAuthnExtensionResultAppId().AppId();
      credential->SetClientExtensionResultAppId(appid);
    }
  }

  mTransaction.ref().mPromise->MaybeResolve(credential);
  ClearTransaction();
}

void
WebAuthnManager::RequestAborted(const uint64_t& aTransactionId,
                                const nsresult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome() && mTransaction.ref().mId == aTransactionId) {
    RejectTransaction(aError);
  }
}

void
WebAuthnManager::Abort()
{
  CancelTransaction(NS_ERROR_DOM_ABORT_ERR);
}

}
}
