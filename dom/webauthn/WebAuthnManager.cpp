/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "hasht.h"
#include "nsNetCID.h"
#include "nsICryptoHash.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/AuthenticatorAttestationResponse.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebAuthnManager.h"
#include "mozilla/dom/WebAuthnUtil.h"
#include "mozilla/dom/PWebAuthnTransaction.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

/***********************************************************************
 * Statics
 **********************************************************************/

namespace {
StaticRefPtr<WebAuthnManager> gWebAuthnManager;
static mozilla::LazyLogModule gWebAuthnManagerLog("webauthnmanager");
}

NS_IMPL_ISUPPORTS(WebAuthnManager, nsIIPCBackgroundChildCreateCallback);

/***********************************************************************
 * Utility Functions
 **********************************************************************/

template<class OOS>
static nsresult
GetAlgorithmName(const OOS& aAlgorithm,
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

  CollectedClientData clientDataObject;
  clientDataObject.mChallenge.Assign(challengeBase64);
  clientDataObject.mOrigin.Assign(aOrigin);
  clientDataObject.mHashAlg.Assign(NS_LITERAL_STRING("S256"));

  nsAutoString temp;
  if (NS_WARN_IF(!clientDataObject.ToJSON(temp))) {
    return NS_ERROR_FAILURE;
  }

  aJsonOut.Assign(NS_ConvertUTF16toUTF8(temp));
  return NS_OK;
}

nsresult
GetOrigin(nsPIDOMWindowInner* aParent,
          /*out*/ nsAString& aOrigin)
{
  nsCOMPtr<nsIDocument> doc = aParent->GetDoc();
  MOZ_ASSERT(doc);

  nsIPrincipal* principal = doc->NodePrincipal();
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

  return NS_OK;
}

nsresult
RelaxSameOrigin(nsPIDOMWindowInner* aParent,
                const nsAString& aInputRpId,
                /* out */ nsACString& aRelaxedRpId)
{
  MOZ_ASSERT(aParent);
  nsCOMPtr<nsIDocument> document = aParent->GetDoc();
  if (!document || !document->IsHTMLDocument()) {
    return NS_ERROR_FAILURE;
  }

  // TODO: Bug 1329764: Invoke the Relax Algorithm, once properly defined
  aRelaxedRpId.Assign(NS_ConvertUTF16toUTF8(aInputRpId));
  return NS_OK;
}

/***********************************************************************
 * WebAuthnManager Implementation
 **********************************************************************/

WebAuthnManager::WebAuthnManager()
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
WebAuthnManager::MaybeClearTransaction()
{
  mClientData.reset();
  mInfo.reset();
  mTransactionPromise = nullptr;
  if (mChild) {
    RefPtr<WebAuthnTransactionChild> c;
    mChild.swap(c);
    c->Send__delete__(c);
  }
}

WebAuthnManager::~WebAuthnManager()
{
  MaybeClearTransaction();
}

already_AddRefed<MozPromise<nsresult, nsresult, false>>
WebAuthnManager::GetOrCreateBackgroundActor()
{
  bool ok = BackgroundChild::GetOrCreateForCurrentThread(this);
  if (NS_WARN_IF(!ok)) {
    ActorFailed();
  }
  return mPBackgroundCreationPromise.Ensure(__func__);
}

//static
WebAuthnManager*
WebAuthnManager::GetOrCreate()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (gWebAuthnManager) {
    return gWebAuthnManager;
  }

  gWebAuthnManager = new WebAuthnManager();
  ClearOnShutdown(&gWebAuthnManager);
  return gWebAuthnManager;
}

//static
WebAuthnManager*
WebAuthnManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());
  return gWebAuthnManager;
}

already_AddRefed<Promise>
WebAuthnManager::MakeCredential(nsPIDOMWindowInner* aParent,
                                const MakeCredentialOptions& aOptions)
{
  MOZ_ASSERT(aParent);

  MaybeClearTransaction();

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aParent);

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if(rv.Failed()) {
    return nullptr;
  }

  nsString origin;
  rv = GetOrigin(aParent, origin);
  if (NS_WARN_IF(rv.Failed())) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  // If timeoutSeconds was specified, check if its value lies within a
  // reasonable range as defined by the platform and if not, correct it to the
  // closest value lying within that range.

  double adjustedTimeout = 30.0;
  if (aOptions.mTimeout.WasPassed()) {
    adjustedTimeout = aOptions.mTimeout.Value();
    adjustedTimeout = std::max(15.0, adjustedTimeout);
    adjustedTimeout = std::min(120.0, adjustedTimeout);
  }

  nsCString rpId;
  if (!aOptions.mRp.mId.WasPassed()) {
    // If rp.id is not specified, then set rpId to callerOrigin, and rpIdHash to
    // the SHA-256 hash of rpId.
    rpId.Assign(NS_ConvertUTF16toUTF8(origin));
  } else {
    // If rpId is specified, then invoke the procedure used for relaxing the
    // same-origin restriction by setting the document.domain attribute, using
    // rpId as the given value but without changing the current document’s
    // domain. If no errors are thrown, set rpId to the value of host as
    // computed by this procedure, and rpIdHash to the SHA-256 hash of rpId.
    // Otherwise, reject promise with a DOMException whose name is
    // "SecurityError", and terminate this algorithm.

    if (NS_FAILED(RelaxSameOrigin(aParent, aOptions.mRp.mId.Value(), rpId))) {
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

  // Process each element of cryptoParameters using the following steps, to
  // produce a new sequence normalizedParameters.
  nsTArray<PublicKeyCredentialParameters> normalizedParams;
  for (size_t a = 0; a < aOptions.mParameters.Length(); ++a) {
    // Let current be the currently selected element of
    // cryptoParameters.

    // If current.type does not contain a PublicKeyCredentialType
    // supported by this implementation, then stop processing current and move
    // on to the next element in cryptoParameters.
    if (aOptions.mParameters[a].mType != PublicKeyCredentialType::Public_key) {
      continue;
    }

    // Let normalizedAlgorithm be the result of normalizing an algorithm using
    // the procedure defined in [WebCryptoAPI], with alg set to
    // current.algorithm and op set to 'generateKey'. If an error occurs during
    // this procedure, then stop processing current and move on to the next
    // element in cryptoParameters.

    nsString algName;
    if (NS_FAILED(GetAlgorithmName(aOptions.mParameters[a].mAlgorithm,
                                   algName))) {
      continue;
    }

    // Add a new object of type PublicKeyCredentialParameters to
    // normalizedParameters, with type set to current.type and algorithm set to
    // normalizedAlgorithm.
    PublicKeyCredentialParameters normalizedObj;
    normalizedObj.mType = aOptions.mParameters[a].mType;
    normalizedObj.mAlgorithm.SetAsString().Assign(algName);

    if (!normalizedParams.AppendElement(normalizedObj, mozilla::fallible)){
      promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
      return promise.forget();
    }
  }

  // If normalizedAlgorithm is empty and cryptoParameters was not empty, cancel
  // the timer started in step 2, reject promise with a DOMException whose name
  // is "NotSupportedError", and terminate this algorithm.
  if (normalizedParams.IsEmpty() && !aOptions.mParameters.IsEmpty()) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return promise.forget();
  }

  // TODO: The following check should not be here. This is checking for
  // parameters specific to the soft key, and should be put in the soft key
  // manager in the parent process. Still need to serialize
  // PublicKeyCredentialParameters first.

  // Check if at least one of the specified combinations of
  // PublicKeyCredentialParameters and cryptographic parameters is supported. If
  // not, return an error code equivalent to NotSupportedError and terminate the
  // operation.

  bool isValidCombination = false;

  for (size_t a = 0; a < normalizedParams.Length(); ++a) {
    if (normalizedParams[a].mType == PublicKeyCredentialType::Public_key &&
        normalizedParams[a].mAlgorithm.IsString() &&
        normalizedParams[a].mAlgorithm.GetAsString().EqualsLiteral(
          WEBCRYPTO_NAMED_CURVE_P256)) {
      isValidCombination = true;
      break;
    }
  }
  if (!isValidCombination) {
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
  srv = AssembleClientData(origin, challenge, clientDataJSON);
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

  nsTArray<WebAuthnScopedCredentialDescriptor> excludeList;
  if (aOptions.mExcludeList.WasPassed()) {
    for (const auto& s: aOptions.mExcludeList.Value()) {
      WebAuthnScopedCredentialDescriptor c;
      CryptoBuffer cb;
      cb.Assign(s.mId);
      c.id() = cb;
      excludeList.AppendElement(c);
    }
  }

  // TODO: Add extension list building
  nsTArray<WebAuthnExtension> extensions;

  WebAuthnTransactionInfo info(rpIdHash,
                               clientDataHash,
                               adjustedTimeout,
                               excludeList,
                               extensions);
  RefPtr<MozPromise<nsresult, nsresult, false>> p = GetOrCreateBackgroundActor();
  p->Then(GetMainThreadSerialEventTarget(), __func__,
          []() {
            WebAuthnManager* mgr = WebAuthnManager::Get();
            if (!mgr) {
              return;
            }
            mgr->StartRegister();
          },
          []() {
            // This case can't actually happen, we'll have crashed if the child
            // failed to create.
          });
  mTransactionPromise = promise;
  mClientData = Some(clientDataJSON);
  mCurrentParent = aParent;
  mInfo = Some(info);
  return promise.forget();
}

void
WebAuthnManager::StartRegister() {
  if (mChild) {
    mChild->SendRequestRegister(mInfo.ref());
  }
}

void
WebAuthnManager::StartSign() {
  if (mChild) {
    mChild->SendRequestSign(mInfo.ref());
  }
}

already_AddRefed<Promise>
WebAuthnManager::GetAssertion(nsPIDOMWindowInner* aParent,
                              const PublicKeyCredentialRequestOptions& aOptions)
{
  MOZ_ASSERT(aParent);

  MaybeClearTransaction();

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aParent);

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if(rv.Failed()) {
    return nullptr;
  }

  nsString origin;
  rv = GetOrigin(aParent, origin);
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

  nsCString rpId;
  if (!aOptions.mRpId.WasPassed()) {
    // If rpId is not specified, then set rpId to callerOrigin, and rpIdHash to
    // the SHA-256 hash of rpId.
    rpId.Assign(NS_ConvertUTF16toUTF8(origin));
  } else {
    // If rpId is specified, then invoke the procedure used for relaxing the
    // same-origin restriction by setting the document.domain attribute, using
    // rpId as the given value but without changing the current document’s
    // domain. If no errors are thrown, set rpId to the value of host as
    // computed by this procedure, and rpIdHash to the SHA-256 hash of rpId.
    // Otherwise, reject promise with a DOMException whose name is
    // "SecurityError", and terminate this algorithm.

    if (NS_FAILED(RelaxSameOrigin(aParent, aOptions.mRpId.Value(), rpId))) {
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
  srv = AssembleClientData(origin, challenge, clientDataJSON);
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
  if (aOptions.mAllowList.Length() < 1) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return promise.forget();
  }

  nsTArray<WebAuthnScopedCredentialDescriptor> allowList;
  for (const auto& s: aOptions.mAllowList) {
    WebAuthnScopedCredentialDescriptor c;
    CryptoBuffer cb;
    cb.Assign(s.mId);
    c.id() = cb;
    allowList.AppendElement(c);
  }

  // TODO: Add extension list building
  // If extensions was specified, process any extensions supported by this
  // client platform, to produce the extension data that needs to be sent to the
  // authenticator. If an error is encountered while processing an extension,
  // skip that extension and do not produce any extension data for it. Call the
  // result of this processing clientExtensions.
  nsTArray<WebAuthnExtension> extensions;

  WebAuthnTransactionInfo info(rpIdHash,
                               clientDataHash,
                               adjustedTimeout,
                               allowList,
                               extensions);
  RefPtr<MozPromise<nsresult, nsresult, false>> p = GetOrCreateBackgroundActor();
  p->Then(GetMainThreadSerialEventTarget(), __func__,
          []() {
            WebAuthnManager* mgr = WebAuthnManager::Get();
            if (!mgr) {
              return;
            }
            mgr->StartSign();
          },
          []() {
            // This case can't actually happen, we'll have crashed if the child
            // failed to create.
          });

  // Only store off the promise if we've succeeded in sending the IPC event.
  mTransactionPromise = promise;
  mClientData = Some(clientDataJSON);
  mCurrentParent = aParent;
  mInfo = Some(info);
  return promise.forget();
}

void
WebAuthnManager::FinishMakeCredential(nsTArray<uint8_t>& aRegBuffer)
{
  MOZ_ASSERT(mTransactionPromise);
  MOZ_ASSERT(mInfo.isSome());

  CryptoBuffer regData;
  if (NS_WARN_IF(!regData.Assign(aRegBuffer.Elements(), aRegBuffer.Length()))) {
    mTransactionPromise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  // Decompose the U2F registration packet
  CryptoBuffer pubKeyBuf;
  CryptoBuffer keyHandleBuf;
  CryptoBuffer attestationCertBuf;
  CryptoBuffer signatureBuf;

  nsresult rv = U2FDecomposeRegistrationResponse(regData, pubKeyBuf, keyHandleBuf,
                                                 attestationCertBuf, signatureBuf);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Cancel(rv);
    return;
  }

  CryptoBuffer clientDataBuf;
  if (!clientDataBuf.Assign(mClientData.ref())) {
    Cancel(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer rpIdHashBuf;
  if (!rpIdHashBuf.Assign(mInfo.ref().RpIdHash())) {
    Cancel(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer authenticatorDataBuf;
  rv = U2FAssembleAuthenticatorData(authenticatorDataBuf, rpIdHashBuf,
                                    signatureBuf);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Cancel(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  // Create a new PublicKeyCredential object and populate its fields with the
  // values returned from the authenticator as well as the clientDataJSON
  // computed earlier.
  RefPtr<AuthenticatorAttestationResponse> attestation =
      new AuthenticatorAttestationResponse(mCurrentParent);
  attestation->SetClientDataJSON(clientDataBuf);
  attestation->SetAttestationObject(regData);

  RefPtr<PublicKeyCredential> credential = new PublicKeyCredential(mCurrentParent);
  credential->SetRawId(keyHandleBuf);
  credential->SetResponse(attestation);

  mTransactionPromise->MaybeResolve(credential);
  MaybeClearTransaction();
}

void
WebAuthnManager::FinishGetAssertion(nsTArray<uint8_t>& aCredentialId,
                                    nsTArray<uint8_t>& aSigBuffer)
{
  MOZ_ASSERT(mTransactionPromise);
  MOZ_ASSERT(mInfo.isSome());

  CryptoBuffer signatureData;
  if (NS_WARN_IF(!signatureData.Assign(aSigBuffer.Elements(), aSigBuffer.Length()))) {
    Cancel(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer clientDataBuf;
  if (!clientDataBuf.Assign(mClientData.ref())) {
    Cancel(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer rpIdHashBuf;
  if (!rpIdHashBuf.Assign(mInfo.ref().RpIdHash())) {
    Cancel(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer authenticatorDataBuf;
  nsresult rv = U2FAssembleAuthenticatorData(authenticatorDataBuf, rpIdHashBuf,
                                             signatureData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Cancel(rv);
    return;
  }

  CryptoBuffer credentialBuf;
  if (!credentialBuf.Assign(aCredentialId)) {
    Cancel(rv);
    return;
  }

  // If any authenticator returns success:

  // Create a new PublicKeyCredential object named value and populate its fields
  // with the values returned from the authenticator as well as the
  // clientDataJSON computed earlier.
  RefPtr<AuthenticatorAssertionResponse> assertion =
    new AuthenticatorAssertionResponse(mCurrentParent);
  assertion->SetClientDataJSON(clientDataBuf);
  assertion->SetAuthenticatorData(authenticatorDataBuf);
  assertion->SetSignature(signatureData);

  RefPtr<PublicKeyCredential> credential =
    new PublicKeyCredential(mCurrentParent);
  credential->SetRawId(credentialBuf);
  credential->SetResponse(assertion);

  mTransactionPromise->MaybeResolve(credential);
  MaybeClearTransaction();
}

void
WebAuthnManager::Cancel(const nsresult& aError)
{
  if (mTransactionPromise) {
    mTransactionPromise->MaybeReject(aError);
  }
  MaybeClearTransaction();
}

void
WebAuthnManager::ActorCreated(PBackgroundChild* aActor)
{
  MOZ_ASSERT(aActor);

  RefPtr<WebAuthnTransactionChild> mgr(new WebAuthnTransactionChild());
  PWebAuthnTransactionChild* constructedMgr =
    aActor->SendPWebAuthnTransactionConstructor(mgr);

  if (NS_WARN_IF(!constructedMgr)) {
    ActorFailed();
    return;
  }
  MOZ_ASSERT(constructedMgr == mgr);
  mChild = mgr.forget();
  mPBackgroundCreationPromise.Resolve(NS_OK, __func__);
}

void
WebAuthnManager::ActorDestroyed()
{
  mChild = nullptr;
}

void
WebAuthnManager::ActorFailed()
{
  MOZ_CRASH("We shouldn't be here!");
}

}
}
