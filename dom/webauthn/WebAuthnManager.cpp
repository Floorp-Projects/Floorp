/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "hasht.h"
#include "nsHTMLDocument.h"
#include "nsIURIMutator.h"
#include "nsThreadUtils.h"
#include "WebAuthnCoseIdentifiers.h"
#include "mozilla/dom/AuthenticatorAttestationResponse.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PWebAuthnTransaction.h"
#include "mozilla/dom/WebAuthnManager.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/dom/WebAuthnUtil.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"

#ifdef OS_WIN
#  include "WinWebAuthnManager.h"
#endif

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

/***********************************************************************
 * Statics
 **********************************************************************/

namespace {
static mozilla::LazyLogModule gWebAuthnManagerLog("webauthnmanager");
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(WebAuthnManager,
                                               WebAuthnManagerBase)

NS_IMPL_CYCLE_COLLECTION_CLASS(WebAuthnManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WebAuthnManager,
                                                WebAuthnManagerBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFollowingSignal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransaction)
  tmp->mTransaction.reset();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WebAuthnManager,
                                                  WebAuthnManagerBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFollowingSignal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransaction)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

/***********************************************************************
 * Utility Functions
 **********************************************************************/

static nsresult AssembleClientData(
    const nsAString& aOrigin, const CryptoBuffer& aChallenge,
    const nsAString& aType,
    const AuthenticationExtensionsClientInputs& aExtensions,
    /* out */ nsACString& aJsonOut) {
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

nsresult GetOrigin(nsPIDOMWindowInner* aParent,
                   /*out*/ nsAString& aOrigin, /*out*/ nsACString& aHost) {
  MOZ_ASSERT(aParent);
  nsCOMPtr<Document> doc = aParent->GetDoc();
  MOZ_ASSERT(doc);

  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  nsresult rv = nsContentUtils::GetUTFOrigin(principal, aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(aOrigin.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  if (aOrigin.EqualsLiteral("null")) {
    // 4.1.1.3 If callerOrigin is an opaque origin, reject promise with a
    // DOMException whose name is "NotAllowedError", and terminate this
    // algorithm
    MOZ_LOG(gWebAuthnManagerLog, LogLevel::Debug,
            ("Rejecting due to opaque origin"));
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

nsresult RelaxSameOrigin(nsPIDOMWindowInner* aParent,
                         const nsAString& aInputRpId,
                         /* out */ nsACString& aRelaxedRpId) {
  MOZ_ASSERT(aParent);
  nsCOMPtr<Document> doc = aParent->GetDoc();
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
  nsCOMPtr<Document> document = aParent->GetDoc();
  if (!document || !document->IsHTMLDocument()) {
    return NS_ERROR_FAILURE;
  }
  nsHTMLDocument* html = document->AsHTMLDocument();
  // See if the given RP ID is a valid domain string.
  // (We use the document's URI here as a template so we don't have to come up
  // with our own scheme, etc. If we can successfully set the host as the given
  // RP ID, then it should be a valid domain string.)
  nsCOMPtr<nsIURI> inputRpIdURI;
  nsresult rv = NS_MutateURI(uri)
                    .SetHost(NS_ConvertUTF16toUTF8(aInputRpId))
                    .Finalize(inputRpIdURI);
  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  nsAutoCString inputRpId;
  if (NS_FAILED(inputRpIdURI->GetAsciiHost(inputRpId))) {
    return NS_ERROR_FAILURE;
  }
  if (!html->IsRegistrableDomainSuffixOfOrEqualTo(
          NS_ConvertUTF8toUTF16(inputRpId), originHost)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  aRelaxedRpId.Assign(inputRpId);
  return NS_OK;
}

/***********************************************************************
 * WebAuthnManager Implementation
 **********************************************************************/

void WebAuthnManager::ClearTransaction() {
  if (!mTransaction.isNothing()) {
    StopListeningForVisibilityEvents();
  }

  mTransaction.reset();
  Unfollow();
}

void WebAuthnManager::RejectTransaction(const nsresult& aError) {
  if (!NS_WARN_IF(mTransaction.isNothing())) {
    mTransaction.ref().mPromise->MaybeReject(aError);
  }

  ClearTransaction();
}

void WebAuthnManager::CancelTransaction(const nsresult& aError) {
  if (!NS_WARN_IF(!mChild || mTransaction.isNothing())) {
    mChild->SendRequestCancel(mTransaction.ref().mId);
  }

  RejectTransaction(aError);
}

void WebAuthnManager::HandleVisibilityChange() {
  if (mTransaction.isSome()) {
    mTransaction.ref().mVisibilityChanged = true;
  }
}

WebAuthnManager::~WebAuthnManager() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome()) {
    ClearTransaction();
  }

  if (mChild) {
    RefPtr<WebAuthnTransactionChild> c;
    mChild.swap(c);
    c->Disconnect();
  }
}

already_AddRefed<Promise> WebAuthnManager::MakeCredential(
    const PublicKeyCredentialCreationOptions& aOptions,
    const Optional<OwningNonNull<AbortSignal>>& aSignal) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  if (mTransaction.isSome()) {
    // If there hasn't been a visibility change during the current
    // transaction, then let's let that one complete rather than
    // cancelling it on a subsequent call.
    if (!mTransaction.ref().mVisibilityChanged) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return promise.forget();
    }

    // Otherwise, the user may well have clicked away, so let's
    // abort the old transaction and take over control from here.
    CancelTransaction(NS_ERROR_ABORT);
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

  CryptoBuffer userId;
  userId.Assign(aOptions.mUser.mId);
  if (userId.Length() > 64) {
    promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR);
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

  // Process each element of mPubKeyCredParams using the following steps, to
  // produce a new sequence coseAlgos.
  nsTArray<CoseAlg> coseAlgos;
  for (size_t a = 0; a < aOptions.mPubKeyCredParams.Length(); ++a) {
    // If current.type does not contain a PublicKeyCredentialType
    // supported by this implementation, then stop processing current and move
    // on to the next element in mPubKeyCredParams.
    if (aOptions.mPubKeyCredParams[a].mType !=
        PublicKeyCredentialType::Public_key) {
      continue;
    }

    coseAlgos.AppendElement(aOptions.mPubKeyCredParams[a].mAlg);
  }

  // If there are algorithms specified, but none are Public_key algorithms,
  // reject the promise.
  if (coseAlgos.IsEmpty() && !aOptions.mPubKeyCredParams.IsEmpty()) {
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
  nsresult srv = AssembleClientData(origin, challenge,
                                    NS_LITERAL_STRING("webauthn.create"),
                                    aOptions.mExtensions, clientDataJSON);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  nsTArray<WebAuthnScopedCredential> excludeList;
  for (const auto& s : aOptions.mExcludeCredentials) {
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

  // Attachment
  Maybe<AuthenticatorAttachment> authenticatorAttachment;
  if (attachment.WasPassed()) {
    authenticatorAttachment.emplace(attachment.Value());
  }

  // Create and forward authenticator selection criteria.
  WebAuthnAuthenticatorSelection authSelection(selection.mRequireResidentKey,
                                               selection.mUserVerification,
                                               authenticatorAttachment);

  nsString rpIcon;
  if (aOptions.mRp.mIcon.WasPassed()) {
    rpIcon = aOptions.mRp.mIcon.Value();
  }

  nsString userIcon;
  if (aOptions.mUser.mIcon.WasPassed()) {
    userIcon = aOptions.mUser.mIcon.Value();
  }

  WebAuthnMakeCredentialRpInfo rpInfo(aOptions.mRp.mName, rpIcon);

  WebAuthnMakeCredentialUserInfo userInfo(
      userId, aOptions.mUser.mName, userIcon, aOptions.mUser.mDisplayName);

  WebAuthnMakeCredentialExtraInfo extra(rpInfo, userInfo, coseAlgos, extensions,
                                        authSelection, attestation);

  WebAuthnMakeCredentialInfo info(origin, NS_ConvertUTF8toUTF16(rpId),
                                  challenge, clientDataJSON, adjustedTimeout,
                                  excludeList, Some(extra));

#ifdef OS_WIN
  if (!WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    ListenForVisibilityEvents();
  }
#else
  ListenForVisibilityEvents();
#endif

  AbortSignal* signal = nullptr;
  if (aSignal.WasPassed()) {
    signal = &aSignal.Value();
    Follow(signal);
  }

  MOZ_ASSERT(mTransaction.isNothing());
  mTransaction = Some(WebAuthnTransaction(promise));
  mChild->SendRequestRegister(mTransaction.ref().mId, info);

  return promise.forget();
}

already_AddRefed<Promise> WebAuthnManager::GetAssertion(
    const PublicKeyCredentialRequestOptions& aOptions,
    const Optional<OwningNonNull<AbortSignal>>& aSignal) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  if (mTransaction.isSome()) {
    // If there hasn't been a visibility change during the current
    // transaction, then let's let that one complete rather than
    // cancelling it on a subsequent call.
    if (!mTransaction.ref().mVisibilityChanged) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return promise.forget();
    }

    // Otherwise, the user may well have clicked away, so let's
    // abort the old transaction and take over control from here.
    CancelTransaction(NS_ERROR_ABORT);
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
  nsresult srv =
      AssembleClientData(origin, challenge, NS_LITERAL_STRING("webauthn.get"),
                         aOptions.mExtensions, clientDataJSON);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  nsTArray<WebAuthnScopedCredential> allowList;
  for (const auto& s : aOptions.mAllowCredentials) {
    if (s.mType == PublicKeyCredentialType::Public_key) {
      WebAuthnScopedCredential c;
      CryptoBuffer cb;
      cb.Assign(s.mId);
      c.id() = cb;

      // Serialize transports.
      if (s.mTransports.WasPassed()) {
        uint8_t transports = 0;
        for (const auto& t : s.mTransports.Value()) {
          if (t == AuthenticatorTransport::Usb) {
            transports |= U2F_AUTHENTICATOR_TRANSPORT_USB;
          }
          if (t == AuthenticatorTransport::Nfc) {
            transports |= U2F_AUTHENTICATOR_TRANSPORT_NFC;
          }
          if (t == AuthenticatorTransport::Ble) {
            transports |= U2F_AUTHENTICATOR_TRANSPORT_BLE;
          }
          if (t == AuthenticatorTransport::Internal) {
            transports |= CTAP_AUTHENTICATOR_TRANSPORT_INTERNAL;
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
    if (!EvaluateAppID(mParent, origin, appId)) {
      promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return promise.forget();
    }

    CryptoBuffer appIdHash;
    if (!appIdHash.SetLength(SHA256_LENGTH, fallible)) {
      promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
      return promise.forget();
    }

    // We need the SHA-256 hash of the appId.
    srv = HashCString(NS_ConvertUTF16toUTF8(appId), appIdHash);
    if (NS_WARN_IF(NS_FAILED(srv))) {
      promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return promise.forget();
    }

    // Append the hash and send it to the backend.
    extensions.AppendElement(WebAuthnExtensionAppId(appIdHash, appId));
  }

  WebAuthnGetAssertionExtraInfo extra(extensions, aOptions.mUserVerification);

  WebAuthnGetAssertionInfo info(origin, NS_ConvertUTF8toUTF16(rpId), challenge,
                                clientDataJSON, adjustedTimeout, allowList,
                                Some(extra));

#ifdef OS_WIN
  if (!WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    ListenForVisibilityEvents();
  }
#else
  ListenForVisibilityEvents();
#endif

  AbortSignal* signal = nullptr;
  if (aSignal.WasPassed()) {
    signal = &aSignal.Value();
    Follow(signal);
  }

  MOZ_ASSERT(mTransaction.isNothing());
  mTransaction = Some(WebAuthnTransaction(promise));
  mChild->SendRequestSign(mTransaction.ref().mId, info);

  return promise.forget();
}

already_AddRefed<Promise> WebAuthnManager::Store(
    const Credential& aCredential) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  if (mTransaction.isSome()) {
    // If there hasn't been a visibility change during the current
    // transaction, then let's let that one complete rather than
    // cancelling it on a subsequent call.
    if (!mTransaction.ref().mVisibilityChanged) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return promise.forget();
    }

    // Otherwise, the user may well have clicked away, so let's
    // abort the old transaction and take over control from here.
    CancelTransaction(NS_ERROR_ABORT);
  }

  promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return promise.forget();
}

void WebAuthnManager::FinishMakeCredential(
    const uint64_t& aTransactionId,
    const WebAuthnMakeCredentialResult& aResult) {
  MOZ_ASSERT(NS_IsMainThread());

  // Check for a valid transaction.
  if (mTransaction.isNothing() || mTransaction.ref().mId != aTransactionId) {
    return;
  }

  CryptoBuffer clientDataBuf;
  if (NS_WARN_IF(!clientDataBuf.Assign(aResult.ClientDataJSON()))) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer attObjBuf;
  if (NS_WARN_IF(!attObjBuf.Assign(aResult.AttestationObject()))) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer keyHandleBuf;
  if (NS_WARN_IF(!keyHandleBuf.Assign(aResult.KeyHandle()))) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsAutoString keyHandleBase64Url;
  nsresult rv = keyHandleBuf.ToJwkBase64(keyHandleBase64Url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectTransaction(rv);
    return;
  }

  // Create a new PublicKeyCredential object and populate its fields with the
  // values returned from the authenticator as well as the clientDataJSON
  // computed earlier.
  RefPtr<AuthenticatorAttestationResponse> attestation =
      new AuthenticatorAttestationResponse(mParent);
  attestation->SetClientDataJSON(clientDataBuf);
  attestation->SetAttestationObject(attObjBuf);

  RefPtr<PublicKeyCredential> credential = new PublicKeyCredential(mParent);
  credential->SetId(keyHandleBase64Url);
  credential->SetType(NS_LITERAL_STRING("public-key"));
  credential->SetRawId(keyHandleBuf);
  credential->SetResponse(attestation);

  mTransaction.ref().mPromise->MaybeResolve(credential);
  ClearTransaction();
}

void WebAuthnManager::FinishGetAssertion(
    const uint64_t& aTransactionId, const WebAuthnGetAssertionResult& aResult) {
  MOZ_ASSERT(NS_IsMainThread());

  // Check for a valid transaction.
  if (mTransaction.isNothing() || mTransaction.ref().mId != aTransactionId) {
    return;
  }

  CryptoBuffer clientDataBuf;
  if (!clientDataBuf.Assign(aResult.ClientDataJSON())) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer credentialBuf;
  if (!credentialBuf.Assign(aResult.KeyHandle())) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer signatureBuf;
  if (!signatureBuf.Assign(aResult.Signature())) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  CryptoBuffer authenticatorDataBuf;
  if (!authenticatorDataBuf.Assign(aResult.AuthenticatorData())) {
    RejectTransaction(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsAutoString credentialBase64Url;
  nsresult rv = credentialBuf.ToJwkBase64(credentialBase64Url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectTransaction(rv);
    return;
  }

  CryptoBuffer userHandleBuf;
  // U2FTokenManager don't return user handle.
  // Best effort.
  userHandleBuf.Assign(aResult.UserHandle());

  // If any authenticator returns success:

  // Create a new PublicKeyCredential object named value and populate its fields
  // with the values returned from the authenticator as well as the
  // clientDataJSON computed earlier.
  RefPtr<AuthenticatorAssertionResponse> assertion =
      new AuthenticatorAssertionResponse(mParent);
  assertion->SetClientDataJSON(clientDataBuf);
  assertion->SetAuthenticatorData(authenticatorDataBuf);
  assertion->SetSignature(signatureBuf);
  if (!userHandleBuf.IsEmpty()) {
    assertion->SetUserHandle(userHandleBuf);
  }

  RefPtr<PublicKeyCredential> credential = new PublicKeyCredential(mParent);
  credential->SetId(credentialBase64Url);
  credential->SetType(NS_LITERAL_STRING("public-key"));
  credential->SetRawId(credentialBuf);
  credential->SetResponse(assertion);

  // Forward client extension results.
  for (auto& ext : aResult.Extensions()) {
    if (ext.type() == WebAuthnExtensionResult::TWebAuthnExtensionResultAppId) {
      bool appid = ext.get_WebAuthnExtensionResultAppId().AppId();
      credential->SetClientExtensionResultAppId(appid);
    }
  }

  mTransaction.ref().mPromise->MaybeResolve(credential);
  ClearTransaction();
}

void WebAuthnManager::RequestAborted(const uint64_t& aTransactionId,
                                     const nsresult& aError) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome() && mTransaction.ref().mId == aTransactionId) {
    RejectTransaction(aError);
  }
}

void WebAuthnManager::Abort() { CancelTransaction(NS_ERROR_DOM_ABORT_ERR); }

}  // namespace dom
}  // namespace mozilla
