/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/U2F.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "nsContentUtils.h"
#include "nsICryptoHash.h"
#include "nsIEffectiveTLDService.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsURLParsers.h"
#include "U2FUtil.h"
#include "hasht.h"

using namespace mozilla::ipc;

// Forward decl because of nsHTMLDocument.h's complex dependency on /layout/style
class nsHTMLDocument {
public:
  bool IsRegistrableDomainSuffixOfOrEqualTo(const nsAString& aHostSuffixString,
                                            const nsACString& aOrigHost);
};

namespace mozilla {
namespace dom {

static mozilla::LazyLogModule gU2FLog("u2fmanager");

NS_NAMED_LITERAL_STRING(kVisibilityChange, "visibilitychange");
NS_NAMED_LITERAL_STRING(kFinishEnrollment, "navigator.id.finishEnrollment");
NS_NAMED_LITERAL_STRING(kGetAssertion, "navigator.id.getAssertion");

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(U2F)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(U2F)
NS_IMPL_CYCLE_COLLECTING_RELEASE(U2F)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(U2F, mParent)

/***********************************************************************
 * Utility Functions
 **********************************************************************/

static ErrorCode
ConvertNSResultToErrorCode(const nsresult& aError)
{
  if (aError == NS_ERROR_DOM_TIMEOUT_ERR) {
    return ErrorCode::TIMEOUT;
  }
  /* Emitted by U2F{Soft,HID}TokenManager when we really mean ineligible */
  if (aError == NS_ERROR_DOM_NOT_ALLOWED_ERR) {
    return ErrorCode::DEVICE_INELIGIBLE;
  }
  return ErrorCode::OTHER_ERROR;
}

static uint32_t
AdjustedTimeoutMillis(const Optional<Nullable<int32_t>>& opt_aSeconds)
{
  uint32_t adjustedTimeoutMillis = 30000u;
  if (opt_aSeconds.WasPassed() && !opt_aSeconds.Value().IsNull()) {
    adjustedTimeoutMillis = opt_aSeconds.Value().Value() * 1000u;
    adjustedTimeoutMillis = std::max(15000u, adjustedTimeoutMillis);
    adjustedTimeoutMillis = std::min(120000u, adjustedTimeoutMillis);
  }
  return adjustedTimeoutMillis;
}

static nsresult
AssembleClientData(const nsAString& aOrigin, const nsAString& aTyp,
                   const nsAString& aChallenge,
                   /* out */ nsString& aClientData)
{
  MOZ_ASSERT(NS_IsMainThread());
  U2FClientData clientDataObject;
  clientDataObject.mTyp.Construct(aTyp); // "Typ" from the U2F specification
  clientDataObject.mChallenge.Construct(aChallenge);
  clientDataObject.mOrigin.Construct(aOrigin);

  if (NS_WARN_IF(!clientDataObject.ToJSON(aClientData))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static void
RegisteredKeysToScopedCredentialList(const nsAString& aAppId,
  const nsTArray<RegisteredKey>& aKeys,
  nsTArray<WebAuthnScopedCredentialDescriptor>& aList)
{
  for (const RegisteredKey& key : aKeys) {
    // Check for required attributes
    if (!key.mVersion.WasPassed() || !key.mKeyHandle.WasPassed() ||
        key.mVersion.Value() != kRequiredU2FVersion) {
      continue;
    }

    // If this key's mAppId doesn't match the invocation, we can't handle it.
    if (key.mAppId.WasPassed() && !key.mAppId.Value().Equals(aAppId)) {
      continue;
    }

    CryptoBuffer keyHandle;
    nsresult rv = keyHandle.FromJwkBase64(key.mKeyHandle.Value());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    WebAuthnScopedCredentialDescriptor c;
    c.id() = keyHandle;
    aList.AppendElement(c);
  }
}

static ErrorCode
EvaluateAppID(nsPIDOMWindowInner* aParent, const nsString& aOrigin,
              /* in/out */ nsString& aAppId)
{
  // Facet is the specification's way of referring to the web origin.
  nsAutoCString facetString = NS_ConvertUTF16toUTF8(aOrigin);
  nsCOMPtr<nsIURI> facetUri;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(facetUri), facetString))) {
    return ErrorCode::BAD_REQUEST;
  }

  // If the facetId (origin) is not HTTPS, reject
  bool facetIsHttps = false;
  if (NS_FAILED(facetUri->SchemeIs("https", &facetIsHttps)) || !facetIsHttps) {
    return ErrorCode::BAD_REQUEST;
  }

  // If the appId is empty or null, overwrite it with the facetId and accept
  if (aAppId.IsEmpty() || aAppId.EqualsLiteral("null")) {
    aAppId.Assign(aOrigin);
    return ErrorCode::OK;
  }

  // AppID is user-supplied. It's quite possible for this parse to fail.
  nsAutoCString appIdString = NS_ConvertUTF16toUTF8(aAppId);
  nsCOMPtr<nsIURI> appIdUri;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(appIdUri), appIdString))) {
    return ErrorCode::BAD_REQUEST;
  }

  // if the appId URL is not HTTPS, reject.
  bool appIdIsHttps = false;
  if (NS_FAILED(appIdUri->SchemeIs("https", &appIdIsHttps)) || !appIdIsHttps) {
    return ErrorCode::BAD_REQUEST;
  }

  nsAutoCString appIdHost;
  if (NS_FAILED(appIdUri->GetAsciiHost(appIdHost))) {
    return ErrorCode::BAD_REQUEST;
  }

  // Allow localhost.
  if (appIdHost.EqualsLiteral("localhost")) {
    nsAutoCString facetHost;
    if (NS_FAILED(facetUri->GetAsciiHost(facetHost))) {
      return ErrorCode::BAD_REQUEST;
    }

    if (facetHost.EqualsLiteral("localhost")) {
      return ErrorCode::OK;
    }
  }

  // Run the HTML5 algorithm to relax the same-origin policy, copied from W3C
  // Web Authentication. See Bug 1244959 comment #8 for context on why we are
  // doing this instead of implementing the external-fetch FacetID logic.
  nsCOMPtr<nsIDocument> document = aParent->GetDoc();
  if (!document || !document->IsHTMLDocument()) {
    return ErrorCode::BAD_REQUEST;
  }
  nsHTMLDocument* html = document->AsHTMLDocument();
  if (NS_WARN_IF(!html)) {
    return ErrorCode::BAD_REQUEST;
  }

  // Use the base domain as the facet for evaluation. This lets this algorithm
  // relax the whole eTLD+1.
  nsCOMPtr<nsIEffectiveTLDService> tldService =
    do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (!tldService) {
    return ErrorCode::BAD_REQUEST;
  }

  nsAutoCString lowestFacetHost;
  if (NS_FAILED(tldService->GetBaseDomain(facetUri, 0, lowestFacetHost))) {
    return ErrorCode::BAD_REQUEST;
  }

  MOZ_LOG(gU2FLog, LogLevel::Debug,
          ("AppId %s Facet %s", appIdHost.get(), lowestFacetHost.get()));

  if (html->IsRegistrableDomainSuffixOfOrEqualTo(NS_ConvertUTF8toUTF16(lowestFacetHost),
                                                 appIdHost)) {
    return ErrorCode::OK;
  }

  return ErrorCode::BAD_REQUEST;
}

static nsresult
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

  if (MOZ_LOG_TEST(gU2FLog, LogLevel::Debug)) {
    nsString base64;
    Unused << NS_WARN_IF(NS_FAILED(aRpIdHash.ToJwkBase64(base64)));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::RpID: %s", aRpId.get()));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::Rp ID Hash (base64): %s",
              NS_ConvertUTF16toUTF8(base64).get()));

    Unused << NS_WARN_IF(NS_FAILED(aClientDataHash.ToJwkBase64(base64)));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::Client Data JSON: %s", aClientDataJSON.get()));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::Client Data Hash (base64): %s",
              NS_ConvertUTF16toUTF8(base64).get()));
  }

  return NS_OK;
}

template<typename T, typename C>
static void
ExecuteCallback(T& aResp, Maybe<nsMainThreadPtrHandle<C>>& aCb)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aCb.isNothing()) {
    return;
  }

  // reset callback earlier to allow reentry from callback.
  nsMainThreadPtrHandle<C> callback(aCb.ref());
  aCb.reset();
  MOZ_ASSERT(aCb.isNothing());
  MOZ_ASSERT(!!callback);

  ErrorResult error;
  callback->Call(aResp, error);
  NS_WARNING_ASSERTION(!error.Failed(), "dom::U2F::Promise callback failed");
  error.SuppressException(); // Useful exceptions already emitted
}

/***********************************************************************
 * U2F JavaScript API Implementation
 **********************************************************************/

U2F::U2F(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
{
  MOZ_ASSERT(NS_IsMainThread());
}

U2F::~U2F()
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

  mRegisterCallback.reset();
  mSignCallback.reset();
}

void
U2F::Init(ErrorResult& aRv)
{
  MOZ_ASSERT(mParent);

  nsCOMPtr<nsIDocument> doc = mParent->GetDoc();
  MOZ_ASSERT(doc);
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIPrincipal* principal = doc->NodePrincipal();
  aRv = nsContentUtils::GetUTFOrigin(principal, mOrigin);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (NS_WARN_IF(mOrigin.IsEmpty())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
}

/* virtual */ JSObject*
U2F::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return U2FBinding::Wrap(aCx, this, aGivenProto);
}

void
U2F::Register(const nsAString& aAppId,
              const Sequence<RegisterRequest>& aRegisterRequests,
              const Sequence<RegisteredKey>& aRegisteredKeys,
              U2FRegisterCallback& aCallback,
              const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
              ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome()) {
    CancelTransaction(NS_ERROR_ABORT);
  }

  MOZ_ASSERT(mRegisterCallback.isNothing());
  mRegisterCallback = Some(nsMainThreadPtrHandle<U2FRegisterCallback>(
                        new nsMainThreadPtrHolder<U2FRegisterCallback>(
                            "U2F::Register::callback", &aCallback)));

  uint32_t adjustedTimeoutMillis = AdjustedTimeoutMillis(opt_aTimeoutSeconds);

  // Evaluate the AppID
  nsString adjustedAppId;
  adjustedAppId.Assign(aAppId);
  ErrorCode appIdResult = EvaluateAppID(mParent, mOrigin, adjustedAppId);
  if (appIdResult != ErrorCode::OK) {
    RegisterResponse response;
    response.mErrorCode.Construct(static_cast<uint32_t>(appIdResult));
    ExecuteCallback(response, mRegisterCallback);
    return;
  }

  // Produce the AppParam from the current AppID
  nsCString cAppId = NS_ConvertUTF16toUTF8(adjustedAppId);

  nsAutoString clientDataJSON;

  // Pick the first valid RegisterRequest; we can only work with one.
  for (const RegisterRequest& req : aRegisterRequests) {
    if (!req.mChallenge.WasPassed() || !req.mVersion.WasPassed() ||
        req.mVersion.Value() != kRequiredU2FVersion) {
      continue;
    }

    nsresult rv = AssembleClientData(mOrigin, kFinishEnrollment,
                                     req.mChallenge.Value(), clientDataJSON);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
  }

  // Did we not get a valid RegisterRequest? Abort.
  if (clientDataJSON.IsEmpty()) {
    RegisterResponse response;
    response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::BAD_REQUEST));
    ExecuteCallback(response, mRegisterCallback);
    return;
  }

  // Build the exclusion list, if any
  nsTArray<WebAuthnScopedCredentialDescriptor> excludeList;
  RegisteredKeysToScopedCredentialList(adjustedAppId, aRegisteredKeys,
                                       excludeList);

  auto clientData = NS_ConvertUTF16toUTF8(clientDataJSON);

  CryptoBuffer rpIdHash, clientDataHash;
  if (NS_FAILED(BuildTransactionHashes(cAppId, clientData,
                                       rpIdHash, clientDataHash))) {
    RegisterResponse response;
    response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OTHER_ERROR));
    ExecuteCallback(response, mRegisterCallback);
    return;
  }

  if (!MaybeCreateBackgroundActor()) {
    RegisterResponse response;
    response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OTHER_ERROR));
    ExecuteCallback(response, mRegisterCallback);
    return;
  }

  ListenForVisibilityEvents();

  // Always blank for U2F
  nsTArray<WebAuthnExtension> extensions;

  // Default values for U2F.
  WebAuthnAuthenticatorSelection authSelection(false /* requireResidentKey */,
                                               false /* requireUserVerification */,
                                               false /* requirePlatformAttachment */);

  WebAuthnMakeCredentialInfo info(rpIdHash,
                                  clientDataHash,
                                  adjustedTimeoutMillis,
                                  excludeList,
                                  extensions,
                                  authSelection);

  MOZ_ASSERT(mTransaction.isNothing());
  mTransaction = Some(U2FTransaction(clientData));
  mChild->SendRequestRegister(mTransaction.ref().mId, info);
}

void
U2F::FinishMakeCredential(const uint64_t& aTransactionId,
                          nsTArray<uint8_t>& aRegBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Check for a valid transaction.
  if (mTransaction.isNothing() || mTransaction.ref().mId != aTransactionId) {
    return;
  }

  CryptoBuffer clientDataBuf;
  if (NS_WARN_IF(!clientDataBuf.Assign(mTransaction.ref().mClientData))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  CryptoBuffer regBuf;
  if (NS_WARN_IF(!regBuf.Assign(aRegBuffer))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  nsString clientDataBase64;
  nsString registrationDataBase64;
  nsresult rvClientData = clientDataBuf.ToJwkBase64(clientDataBase64);
  nsresult rvRegistrationData = regBuf.ToJwkBase64(registrationDataBase64);

  if (NS_WARN_IF(NS_FAILED(rvClientData)) ||
      NS_WARN_IF(NS_FAILED(rvRegistrationData))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  // Assemble a response object to return
  RegisterResponse response;
  response.mVersion.Construct(kRequiredU2FVersion);
  response.mClientData.Construct(clientDataBase64);
  response.mRegistrationData.Construct(registrationDataBase64);
  response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

  ExecuteCallback(response, mRegisterCallback);
  ClearTransaction();
}

void
U2F::Sign(const nsAString& aAppId,
          const nsAString& aChallenge,
          const Sequence<RegisteredKey>& aRegisteredKeys,
          U2FSignCallback& aCallback,
          const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
          ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome()) {
    CancelTransaction(NS_ERROR_ABORT);
  }

  MOZ_ASSERT(mSignCallback.isNothing());
  mSignCallback = Some(nsMainThreadPtrHandle<U2FSignCallback>(
                    new nsMainThreadPtrHolder<U2FSignCallback>(
                        "U2F::Sign::callback", &aCallback)));

  uint32_t adjustedTimeoutMillis = AdjustedTimeoutMillis(opt_aTimeoutSeconds);

  // Evaluate the AppID
  nsString adjustedAppId;
  adjustedAppId.Assign(aAppId);
  ErrorCode appIdResult = EvaluateAppID(mParent, mOrigin, adjustedAppId);
  if (appIdResult != ErrorCode::OK) {
    SignResponse response;
    response.mErrorCode.Construct(static_cast<uint32_t>(appIdResult));
    ExecuteCallback(response, mSignCallback);
    return;
  }

  // Produce the AppParam from the current AppID
  nsCString cAppId = NS_ConvertUTF16toUTF8(adjustedAppId);

  nsAutoString clientDataJSON;
  nsresult rv = AssembleClientData(mOrigin, kGetAssertion, aChallenge,
                                   clientDataJSON);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    SignResponse response;
    response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::BAD_REQUEST));
    ExecuteCallback(response, mSignCallback);
    return;
  }

  // Build the key list, if any
  nsTArray<WebAuthnScopedCredentialDescriptor> permittedList;
  RegisteredKeysToScopedCredentialList(adjustedAppId, aRegisteredKeys,
                                       permittedList);

  auto clientData = NS_ConvertUTF16toUTF8(clientDataJSON);

  CryptoBuffer rpIdHash, clientDataHash;
  if (NS_FAILED(BuildTransactionHashes(cAppId, clientData,
                                       rpIdHash, clientDataHash))) {
    SignResponse response;
    response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OTHER_ERROR));
    ExecuteCallback(response, mSignCallback);
    return;
  }

  if (!MaybeCreateBackgroundActor()) {
    SignResponse response;
    response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OTHER_ERROR));
    ExecuteCallback(response, mSignCallback);
    return;
  }

  ListenForVisibilityEvents();

  // Always blank for U2F
  nsTArray<WebAuthnExtension> extensions;

  WebAuthnGetAssertionInfo info(rpIdHash,
                                clientDataHash,
                                adjustedTimeoutMillis,
                                permittedList,
                                extensions);

  MOZ_ASSERT(mTransaction.isNothing());
  mTransaction = Some(U2FTransaction(clientData));
  mChild->SendRequestSign(mTransaction.ref().mId, info);
}

void
U2F::FinishGetAssertion(const uint64_t& aTransactionId,
                        nsTArray<uint8_t>& aCredentialId,
                        nsTArray<uint8_t>& aSigBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Check for a valid transaction.
  if (mTransaction.isNothing() || mTransaction.ref().mId != aTransactionId) {
    return;
  }

  CryptoBuffer clientDataBuf;
  if (NS_WARN_IF(!clientDataBuf.Assign(mTransaction.ref().mClientData))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  CryptoBuffer credBuf;
  if (NS_WARN_IF(!credBuf.Assign(aCredentialId))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  CryptoBuffer sigBuf;
  if (NS_WARN_IF(!sigBuf.Assign(aSigBuffer))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  // Assemble a response object to return
  nsString clientDataBase64;
  nsString signatureDataBase64;
  nsString keyHandleBase64;
  nsresult rvClientData = clientDataBuf.ToJwkBase64(clientDataBase64);
  nsresult rvSignatureData = sigBuf.ToJwkBase64(signatureDataBase64);
  nsresult rvKeyHandle = credBuf.ToJwkBase64(keyHandleBase64);
  if (NS_WARN_IF(NS_FAILED(rvClientData)) ||
      NS_WARN_IF(NS_FAILED(rvSignatureData) ||
      NS_WARN_IF(NS_FAILED(rvKeyHandle)))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  SignResponse response;
  response.mKeyHandle.Construct(keyHandleBase64);
  response.mClientData.Construct(clientDataBase64);
  response.mSignatureData.Construct(signatureDataBase64);
  response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

  ExecuteCallback(response, mSignCallback);
  ClearTransaction();
}

void
U2F::ClearTransaction()
{
  if (!NS_WARN_IF(mTransaction.isNothing())) {
    StopListeningForVisibilityEvents();
  }

  mTransaction.reset();
}

void
U2F::RejectTransaction(const nsresult& aError)
{
  if (!NS_WARN_IF(mTransaction.isNothing())) {
    ErrorCode code = ConvertNSResultToErrorCode(aError);

    if (mRegisterCallback.isSome()) {
      RegisterResponse response;
      response.mErrorCode.Construct(static_cast<uint32_t>(code));
      ExecuteCallback(response, mRegisterCallback);
    }

    if (mSignCallback.isSome()) {
      SignResponse response;
      response.mErrorCode.Construct(static_cast<uint32_t>(code));
      ExecuteCallback(response, mSignCallback);
    }
  }

  ClearTransaction();
}

void
U2F::CancelTransaction(const nsresult& aError)
{
  if (!NS_WARN_IF(!mChild || mTransaction.isNothing())) {
    mChild->SendRequestCancel(mTransaction.ref().mId);
  }

  RejectTransaction(aError);
}

void
U2F::RequestAborted(const uint64_t& aTransactionId, const nsresult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome() && mTransaction.ref().mId == aTransactionId) {
    RejectTransaction(aError);
  }
}

/***********************************************************************
 * Event Handling
 **********************************************************************/

void
U2F::ListenForVisibilityEvents()
{
  nsCOMPtr<nsIDocument> doc = mParent->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  nsresult rv = doc->AddSystemEventListener(kVisibilityChange, this,
                                            /* use capture */ true,
                                            /* wants untrusted */ false);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void
U2F::StopListeningForVisibilityEvents()
{
  nsCOMPtr<nsIDocument> doc = mParent->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  nsresult rv = doc->RemoveSystemEventListener(kVisibilityChange, this,
                                               /* use capture */ true);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}


NS_IMETHODIMP
U2F::HandleEvent(nsIDOMEvent* aEvent)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aEvent);

  nsAutoString type;
  aEvent->GetType(type);
  if (!type.Equals(kVisibilityChange)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc =
    do_QueryInterface(aEvent->InternalDOMEvent()->GetTarget());
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }

  if (doc->Hidden()) {
    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("Visibility change: U2F window is hidden, cancelling job."));

    CancelTransaction(NS_ERROR_ABORT);
  }

  return NS_OK;
}

/***********************************************************************
 * IPC Protocol Implementation
 **********************************************************************/

bool
U2F::MaybeCreateBackgroundActor()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mChild) {
    return true;
  }

  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    return false;
  }

  RefPtr<WebAuthnTransactionChild> mgr(new WebAuthnTransactionChild(this));
  PWebAuthnTransactionChild* constructedMgr =
    actorChild->SendPWebAuthnTransactionConstructor(mgr);

  if (NS_WARN_IF(!constructedMgr)) {
    return false;
  }

  MOZ_ASSERT(constructedMgr == mgr);
  mChild = mgr.forget();

  return true;
}

void
U2F::ActorDestroyed()
{
  MOZ_ASSERT(NS_IsMainThread());
  mChild = nullptr;
}

} // namespace dom
} // namespace mozilla
