/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/U2F.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "nsContentUtils.h"
#include "nsIEffectiveTLDService.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsURLParsers.h"
#include "U2FManager.h"

// Forward decl because of nsHTMLDocument.h's complex dependency on /layout/style
class nsHTMLDocument {
public:
  bool IsRegistrableDomainSuffixOfOrEqualTo(const nsAString& aHostSuffixString,
                                            const nsACString& aOrigHost);
};

namespace mozilla {
namespace dom {

static mozilla::LazyLogModule gU2FLog("u2fmanager");

NS_NAMED_LITERAL_STRING(kFinishEnrollment, "navigator.id.finishEnrollment");
NS_NAMED_LITERAL_STRING(kGetAssertion, "navigator.id.getAssertion");

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(U2F)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(U2F)
NS_IMPL_CYCLE_COLLECTING_RELEASE(U2F)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(U2F, mParent)

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
  nsAutoCString appIdHost;
  if (NS_FAILED(appIdUri->GetAsciiHost(appIdHost))) {
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

template<typename T, typename C>
static void
ExecuteCallback(T& aResp, Maybe<nsMainThreadPtrHandle<C>>& aCb)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aCb.isNothing()) {
    return;
  }

  ErrorResult error;
  aCb.ref()->Call(aResp, error);
  NS_WARNING_ASSERTION(!error.Failed(), "dom::U2F::Promise callback failed");
  error.SuppressException(); // Useful exceptions already emitted

  aCb.reset();
  MOZ_ASSERT(aCb.isNothing());
}

U2F::U2F(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
{
}

U2F::~U2F()
{
  mPromiseHolder.DisconnectIfExists();
  mRegisterCallback.reset();
  mSignCallback.reset();
}

void
U2F::Init(ErrorResult& aRv)
{
  MOZ_ASSERT(mParent);
  MOZ_ASSERT(!mEventTarget);

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

  mEventTarget = doc->EventTargetFor(TaskCategory::Other);
  MOZ_ASSERT(mEventTarget);
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

  Cancel();

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

  auto& localReqHolder = mPromiseHolder;
  auto& localCb = mRegisterCallback;
  RefPtr<U2FManager> mgr = U2FManager::GetOrCreate();
  RefPtr<U2FPromise> p = mgr->Register(mParent, cAppId,
                                       NS_ConvertUTF16toUTF8(clientDataJSON),
                                       adjustedTimeoutMillis, excludeList);
  p->Then(mEventTarget, "dom::U2F::Register::Promise::Resolve",
          [&localCb, &localReqHolder](nsString aResponse) {
              MOZ_LOG(gU2FLog, LogLevel::Debug,
                      ("dom::U2F::Register::Promise::Resolve, response was %s",
                        NS_ConvertUTF16toUTF8(aResponse).get()));
              RegisterResponse response;
              response.Init(aResponse);

              ExecuteCallback(response, localCb);
              localReqHolder.Complete();
          },
          [&localCb, &localReqHolder](ErrorCode aErrorCode) {
              MOZ_LOG(gU2FLog, LogLevel::Debug,
                      ("dom::U2F::Register::Promise::Reject, response was %d",
                        static_cast<uint32_t>(aErrorCode)));
              RegisterResponse response;
              response.mErrorCode.Construct(static_cast<uint32_t>(aErrorCode));

              ExecuteCallback(response, localCb);
              localReqHolder.Complete();
          })
  ->Track(mPromiseHolder);
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

  Cancel();

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
  auto& localReqHolder = mPromiseHolder;
  auto& localCb = mSignCallback;
  RefPtr<U2FManager> mgr = U2FManager::GetOrCreate();
  RefPtr<U2FPromise> p = mgr->Sign(mParent, cAppId,
                                   NS_ConvertUTF16toUTF8(clientDataJSON),
                                   adjustedTimeoutMillis, permittedList);
  p->Then(mEventTarget, "dom::U2F::Sign::Promise::Resolve",
          [&localCb, &localReqHolder](nsString aResponse) {
              MOZ_LOG(gU2FLog, LogLevel::Debug,
                      ("dom::U2F::Sign::Promise::Resolve, response was %s",
                        NS_ConvertUTF16toUTF8(aResponse).get()));
              SignResponse response;
              response.Init(aResponse);

              ExecuteCallback(response, localCb);
              localReqHolder.Complete();
          },
          [&localCb, &localReqHolder](ErrorCode aErrorCode) {
              MOZ_LOG(gU2FLog, LogLevel::Debug,
                      ("dom::U2F::Sign::Promise::Reject, response was %d",
                        static_cast<uint32_t>(aErrorCode)));
              SignResponse response;
              response.mErrorCode.Construct(static_cast<uint32_t>(aErrorCode));

              ExecuteCallback(response, localCb);
              localReqHolder.Complete();
          })
  ->Track(mPromiseHolder);
}

void
U2F::Cancel()
{
  MOZ_ASSERT(NS_IsMainThread());

  const ErrorCode errorCode = ErrorCode::OTHER_ERROR;

  if (mRegisterCallback.isSome()) {
    RegisterResponse response;
    response.mErrorCode.Construct(static_cast<uint32_t>(errorCode));
    ExecuteCallback(response, mRegisterCallback);
  }

  if (mSignCallback.isSome()) {
    SignResponse response;
    response.mErrorCode.Construct(static_cast<uint32_t>(errorCode));
    ExecuteCallback(response, mSignCallback);
  }

  RefPtr<U2FManager> mgr = U2FManager::Get();
  if (mgr) {
    mgr->Cancel(NS_ERROR_DOM_OPERATION_ERR);
  }

  mPromiseHolder.DisconnectIfExists();
}

} // namespace dom
} // namespace mozilla
