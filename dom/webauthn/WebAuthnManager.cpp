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
#include "WebAuthnEnumStrings.h"
#include "WebAuthnTransportIdentifiers.h"
#include "mozilla/Base64.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/AuthenticatorAssertionResponse.h"
#include "mozilla/dom/AuthenticatorAttestationResponse.h"
#include "mozilla/dom/PublicKeyCredential.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PWebAuthnTransaction.h"
#include "mozilla/dom/PWebAuthnTransactionChild.h"
#include "mozilla/dom/WebAuthnManager.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/dom/WebAuthnUtil.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"

#ifdef XP_WIN
#  include "WinWebAuthnService.h"
#endif

using namespace mozilla::ipc;

namespace mozilla::dom {

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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransaction)
  tmp->mTransaction.reset();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WebAuthnManager,
                                                  WebAuthnManagerBase)
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
  nsresult rv =
      nsContentUtils::GetWebExposedOriginSerialization(principal, aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(aOrigin.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  if (principal->GetIsIpAddress()) {
    return NS_ERROR_DOM_SECURITY_ERR;
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
  auto* basePrin = BasePrincipal::Cast(principal);
  if (NS_FAILED(basePrin->GetURI(getter_AddRefs(originUri)))) {
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
  auto* basePrin = BasePrincipal::Cast(principal);
  nsCOMPtr<nsIURI> uri;

  if (NS_FAILED(basePrin->GetURI(getter_AddRefs(uri)))) {
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
  if (mTransaction.isSome()) {
    StopListeningForVisibilityEvents();
  }

  mTransaction.reset();
  Unfollow();
}

void WebAuthnManager::CancelParent() {
  if (!NS_WARN_IF(!mChild || mTransaction.isNothing())) {
    mChild->SendRequestCancel(mTransaction.ref().mId);
  }
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
    const Optional<OwningNonNull<AbortSignal>>& aSignal, ErrorResult& aError) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  RefPtr<Promise> promise = Promise::Create(global, aError);
  if (aError.Failed()) {
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
    CancelTransaction(NS_ERROR_DOM_ABORT_ERR);
  }

  nsString origin;
  nsCString rpId;
  nsresult rv = GetOrigin(mParent, origin, rpId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  // Enforce 5.4.3 User Account Parameters for Credential Generation
  // When we add UX, we'll want to do more with this value, but for now
  // we just have to verify its correctness.

  CryptoBuffer userId;
  userId.Assign(aOptions.mUser.mId);
  if (userId.Length() > 64) {
    promise->MaybeRejectWithTypeError("user.id is too long");
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
  // produce a new sequence of coseAlgos.
  nsTArray<CoseAlg> coseAlgos;
  // If pubKeyCredParams is empty, append ES256 and RS256
  if (aOptions.mPubKeyCredParams.IsEmpty()) {
    coseAlgos.AppendElement(static_cast<long>(CoseAlgorithmIdentifier::ES256));
    coseAlgos.AppendElement(static_cast<long>(CoseAlgorithmIdentifier::RS256));
  } else {
    for (size_t a = 0; a < aOptions.mPubKeyCredParams.Length(); ++a) {
      // If current.type does not contain a PublicKeyCredentialType
      // supported by this implementation, then stop processing current and move
      // on to the next element in mPubKeyCredParams.
      if (!aOptions.mPubKeyCredParams[a].mType.EqualsLiteral(
              MOZ_WEBAUTHN_PUBLIC_KEY_CREDENTIAL_TYPE_PUBLIC_KEY)) {
        continue;
      }

      coseAlgos.AppendElement(aOptions.mPubKeyCredParams[a].mAlg);
    }
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
  nsresult srv = AssembleClientData(origin, challenge, u"webauthn.create"_ns,
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

  // <https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#sctn-hmac-secret-extension>
  if (aOptions.mExtensions.mHmacCreateSecret.WasPassed()) {
    bool hmacCreateSecret = aOptions.mExtensions.mHmacCreateSecret.Value();
    if (hmacCreateSecret) {
      extensions.AppendElement(WebAuthnExtensionHmacSecret(hmacCreateSecret));
    }
  }

  if (aOptions.mExtensions.mCredProps.WasPassed()) {
    bool credProps = aOptions.mExtensions.mCredProps.Value();
    if (credProps) {
      extensions.AppendElement(WebAuthnExtensionCredProps(credProps));
    }
  }

  if (aOptions.mExtensions.mMinPinLength.WasPassed()) {
    bool minPinLength = aOptions.mExtensions.mMinPinLength.Value();
    if (minPinLength) {
      extensions.AppendElement(WebAuthnExtensionMinPinLength(minPinLength));
    }
  }

  const auto& selection = aOptions.mAuthenticatorSelection;
  const auto& attachment = selection.mAuthenticatorAttachment;
  const nsString& attestation = aOptions.mAttestation;

  // Attachment
  Maybe<nsString> authenticatorAttachment;
  if (attachment.WasPassed()) {
    authenticatorAttachment.emplace(attachment.Value());
  }

  // The residentKey field was added in WebAuthn level 2. It takes precedent
  // over the requireResidentKey field if and only if it is present and it is a
  // member of the ResidentKeyRequirement enum.
  static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
  bool useResidentKeyValue =
      selection.mResidentKey.WasPassed() &&
      (selection.mResidentKey.Value().EqualsLiteral(
           MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_REQUIRED) ||
       selection.mResidentKey.Value().EqualsLiteral(
           MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_PREFERRED) ||
       selection.mResidentKey.Value().EqualsLiteral(
           MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_DISCOURAGED));

  nsString residentKey;
  if (useResidentKeyValue) {
    residentKey = selection.mResidentKey.Value();
  } else {
    // "If no value is given then the effective value is required if
    // requireResidentKey is true or discouraged if it is false or absent."
    if (selection.mRequireResidentKey) {
      residentKey.AssignLiteral(MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_REQUIRED);
    } else {
      residentKey.AssignLiteral(
          MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_DISCOURAGED);
    }
  }

  // Create and forward authenticator selection criteria.
  WebAuthnAuthenticatorSelection authSelection(
      residentKey, selection.mUserVerification, authenticatorAttachment);

  WebAuthnMakeCredentialRpInfo rpInfo(aOptions.mRp.mName);

  WebAuthnMakeCredentialUserInfo userInfo(userId, aOptions.mUser.mName,
                                          aOptions.mUser.mDisplayName);

  BrowsingContext* context = mParent->GetBrowsingContext();
  if (!context) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return promise.forget();
  }

  // Abort the request if aborted flag is already set.
  if (aSignal.WasPassed() && aSignal.Value().Aborted()) {
    AutoJSAPI jsapi;
    if (!jsapi.Init(global)) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return promise.forget();
    }
    JSContext* cx = jsapi.cx();
    JS::Rooted<JS::Value> reason(cx);
    aSignal.Value().GetReason(cx, &reason);
    promise->MaybeReject(reason);
    return promise.forget();
  }

  WebAuthnMakeCredentialInfo info(
      origin, NS_ConvertUTF8toUTF16(rpId), challenge, clientDataJSON,
      adjustedTimeout, excludeList, rpInfo, userInfo, coseAlgos, extensions,
      authSelection, attestation, context->Top()->Id());

  // Set up the transaction state (including event listeners, etc). Fallible
  // operations should not be performed below this line, as we must not leave
  // the transaction state partially initialized. Once the transaction state is
  // initialized the only valid ways to end the transaction are
  // CancelTransaction, RejectTransaction, and FinishMakeCredential.
#ifdef XP_WIN
  if (!WinWebAuthnService::AreWebAuthNApisAvailable()) {
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

const size_t MAX_ALLOWED_CREDENTIALS = 20;

already_AddRefed<Promise> WebAuthnManager::GetAssertion(
    const PublicKeyCredentialRequestOptions& aOptions,
    const Optional<OwningNonNull<AbortSignal>>& aSignal, ErrorResult& aError) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  RefPtr<Promise> promise = Promise::Create(global, aError);
  if (aError.Failed()) {
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
    CancelTransaction(NS_ERROR_DOM_ABORT_ERR);
  }

  nsString origin;
  nsCString rpId;
  nsresult rv = GetOrigin(mParent, origin, rpId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
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

  // Abort the request if the allowCredentials set is too large
  if (aOptions.mAllowCredentials.Length() > MAX_ALLOWED_CREDENTIALS) {
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
  rv = AssembleClientData(origin, challenge, u"webauthn.get"_ns,
                          aOptions.mExtensions, clientDataJSON);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  nsTArray<WebAuthnScopedCredential> allowList;
  for (const auto& s : aOptions.mAllowCredentials) {
    if (s.mType.EqualsLiteral(
            MOZ_WEBAUTHN_PUBLIC_KEY_CREDENTIAL_TYPE_PUBLIC_KEY)) {
      WebAuthnScopedCredential c;
      CryptoBuffer cb;
      cb.Assign(s.mId);
      c.id() = cb;

      // Serialize transports.
      if (s.mTransports.WasPassed()) {
        uint8_t transports = 0;

        // We ignore unknown transports for forward-compatibility, but this
        // needs to be reviewed if values are added to the
        // AuthenticatorTransport enum.
        static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
        for (const nsAString& str : s.mTransports.Value()) {
          if (str.EqualsLiteral(MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_USB)) {
            transports |= MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_USB;
          } else if (str.EqualsLiteral(
                         MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_NFC)) {
            transports |= MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_NFC;
          } else if (str.EqualsLiteral(
                         MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_BLE)) {
            transports |= MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_BLE;
          } else if (str.EqualsLiteral(
                         MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_INTERNAL)) {
            transports |= MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_INTERNAL;
          }
        }
        c.transports() = transports;
      }

      allowList.AppendElement(c);
    }
  }
  if (allowList.Length() == 0 && aOptions.mAllowCredentials.Length() != 0) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return promise.forget();
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

  // credProps is only supported in MakeCredentials
  if (aOptions.mExtensions.mCredProps.WasPassed()) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return promise.forget();
  }

  // minPinLength is only supported in MakeCredentials
  if (aOptions.mExtensions.mMinPinLength.WasPassed()) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return promise.forget();
  }

  // <https://w3c.github.io/webauthn/#sctn-appid-extension>
  if (aOptions.mExtensions.mAppid.WasPassed()) {
    nsString appId(aOptions.mExtensions.mAppid.Value());

    // Check that the appId value is allowed.
    if (!EvaluateAppID(mParent, origin, appId)) {
      promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return promise.forget();
    }

    // Append the hash and send it to the backend.
    extensions.AppendElement(WebAuthnExtensionAppId(appId));
  }

  BrowsingContext* context = mParent->GetBrowsingContext();
  if (!context) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return promise.forget();
  }

  // Abort the request if aborted flag is already set.
  if (aSignal.WasPassed() && aSignal.Value().Aborted()) {
    AutoJSAPI jsapi;
    if (!jsapi.Init(global)) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return promise.forget();
    }
    JSContext* cx = jsapi.cx();
    JS::Rooted<JS::Value> reason(cx);
    aSignal.Value().GetReason(cx, &reason);
    promise->MaybeReject(reason);
    return promise.forget();
  }

  WebAuthnGetAssertionInfo info(origin, NS_ConvertUTF8toUTF16(rpId), challenge,
                                clientDataJSON, adjustedTimeout, allowList,
                                extensions, aOptions.mUserVerification,
                                context->Top()->Id());

  // Set up the transaction state (including event listeners, etc). Fallible
  // operations should not be performed below this line, as we must not leave
  // the transaction state partially initialized. Once the transaction state is
  // initialized the only valid ways to end the transaction are
  // CancelTransaction, RejectTransaction, and FinishGetAssertion.
#ifdef XP_WIN
  if (!WinWebAuthnService::AreWebAuthNApisAvailable()) {
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

already_AddRefed<Promise> WebAuthnManager::Store(const Credential& aCredential,
                                                 ErrorResult& aError) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  RefPtr<Promise> promise = Promise::Create(global, aError);
  if (aError.Failed()) {
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
    CancelTransaction(NS_ERROR_DOM_ABORT_ERR);
  }

  promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return promise.forget();
}

already_AddRefed<Promise> WebAuthnManager::IsUVPAA(GlobalObject& aGlobal,
                                                   ErrorResult& aError) {
  RefPtr<Promise> promise =
      Promise::Create(xpc::CurrentNativeGlobal(aGlobal.Context()), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  if (!MaybeCreateBackgroundActor()) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return promise.forget();
  }

  mChild->SendRequestIsUVPAA()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise](const PWebAuthnTransactionChild::RequestIsUVPAAPromise::
                    ResolveOrRejectValue& aValue) {
        if (aValue.IsResolve()) {
          promise->MaybeResolve(aValue.ResolveValue());
        } else {
          promise->MaybeReject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
        }
      });
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

  nsAutoCString keyHandleBase64Url;
  nsresult rv = Base64URLEncode(
      aResult.KeyHandle().Length(), aResult.KeyHandle().Elements(),
      Base64URLEncodePaddingPolicy::Omit, keyHandleBase64Url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectTransaction(rv);
    return;
  }

  // Create a new PublicKeyCredential object and populate its fields with the
  // values returned from the authenticator as well as the clientDataJSON
  // computed earlier.
  RefPtr<AuthenticatorAttestationResponse> attestation =
      new AuthenticatorAttestationResponse(mParent);
  attestation->SetClientDataJSON(aResult.ClientDataJSON());
  attestation->SetAttestationObject(aResult.AttestationObject());
  attestation->SetTransports(aResult.Transports());

  RefPtr<PublicKeyCredential> credential = new PublicKeyCredential(mParent);
  credential->SetId(NS_ConvertASCIItoUTF16(keyHandleBase64Url));
  credential->SetType(u"public-key"_ns);
  credential->SetRawId(aResult.KeyHandle());
  credential->SetAttestationResponse(attestation);
  credential->SetAuthenticatorAttachment(aResult.AuthenticatorAttachment());

  // Forward client extension results.
  for (const auto& ext : aResult.Extensions()) {
    if (ext.type() ==
        WebAuthnExtensionResult::TWebAuthnExtensionResultCredProps) {
      bool credPropsRk = ext.get_WebAuthnExtensionResultCredProps().rk();
      credential->SetClientExtensionResultCredPropsRk(credPropsRk);
    }
    if (ext.type() ==
        WebAuthnExtensionResult::TWebAuthnExtensionResultHmacSecret) {
      bool hmacCreateSecret =
          ext.get_WebAuthnExtensionResultHmacSecret().hmacCreateSecret();
      credential->SetClientExtensionResultHmacSecret(hmacCreateSecret);
    }
  }

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

  nsAutoCString keyHandleBase64Url;
  nsresult rv = Base64URLEncode(
      aResult.KeyHandle().Length(), aResult.KeyHandle().Elements(),
      Base64URLEncodePaddingPolicy::Omit, keyHandleBase64Url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectTransaction(rv);
    return;
  }

  // Create a new PublicKeyCredential object named value and populate its fields
  // with the values returned from the authenticator as well as the
  // clientDataJSON computed earlier.
  RefPtr<AuthenticatorAssertionResponse> assertion =
      new AuthenticatorAssertionResponse(mParent);
  assertion->SetClientDataJSON(aResult.ClientDataJSON());
  assertion->SetAuthenticatorData(aResult.AuthenticatorData());
  assertion->SetSignature(aResult.Signature());
  assertion->SetUserHandle(aResult.UserHandle());  // may be empty

  RefPtr<PublicKeyCredential> credential = new PublicKeyCredential(mParent);
  credential->SetId(NS_ConvertASCIItoUTF16(keyHandleBase64Url));
  credential->SetType(u"public-key"_ns);
  credential->SetRawId(aResult.KeyHandle());
  credential->SetAssertionResponse(assertion);
  credential->SetAuthenticatorAttachment(aResult.AuthenticatorAttachment());

  // Forward client extension results.
  for (const auto& ext : aResult.Extensions()) {
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

void WebAuthnManager::RunAbortAlgorithm() {
  if (NS_WARN_IF(mTransaction.isNothing())) {
    return;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  AutoJSAPI jsapi;
  if (!jsapi.Init(global)) {
    CancelTransaction(NS_ERROR_DOM_ABORT_ERR);
    return;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> reason(cx);
  Signal()->GetReason(cx, &reason);
  CancelTransaction(reason);
}

}  // namespace mozilla::dom
