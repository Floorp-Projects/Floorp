/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Request.h"

#include "nsIURI.h"
#include "nsPIDOMWindow.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/URL.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/Unused.h"

#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(Request)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Request)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Request, mOwner, mHeaders)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Request)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Request::Request(nsIGlobalObject* aOwner, InternalRequest* aRequest)
  : FetchBody<Request>(aOwner)
  , mRequest(aRequest)
{
  MOZ_ASSERT(aRequest->Headers()->Guard() == HeadersGuardEnum::Immutable ||
             aRequest->Headers()->Guard() == HeadersGuardEnum::Request ||
             aRequest->Headers()->Guard() == HeadersGuardEnum::Request_no_cors);
  SetMimeType();
}

Request::~Request()
{
}

// static
bool
Request::RequestContextEnabled(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.requestcontext.enabled", false);
  }

  using namespace workers;

  // Otherwise, check the pref via the WorkerPrivate
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->RequestContextEnabled();
}

already_AddRefed<InternalRequest>
Request::GetInternalRequest()
{
  RefPtr<InternalRequest> r = mRequest;
  return r.forget();
}

namespace {
already_AddRefed<nsIURI>
ParseURLFromDocument(nsIDocument* aDocument, const nsAString& aInput,
                     ErrorResult& aRv)
{
  MOZ_ASSERT(aDocument);
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> baseURI = aDocument->GetBaseURI();
  nsCOMPtr<nsIURI> resolvedURI;
  aRv = NS_NewURI(getter_AddRefs(resolvedURI), aInput, nullptr, baseURI);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aInput);
  }
  return resolvedURI.forget();
}
void
GetRequestURLFromDocument(nsIDocument* aDocument, const nsAString& aInput,
                          nsAString& aRequestURL, nsACString& aURLfragment,
                          ErrorResult& aRv)
{
  nsCOMPtr<nsIURI> resolvedURI = ParseURLFromDocument(aDocument, aInput, aRv);
  if (aRv.Failed()) {
    return;
  }
  // This fails with URIs with weird protocols, even when they are valid,
  // so we ignore the failure
  nsAutoCString credentials;
  Unused << resolvedURI->GetUserPass(credentials);
  if (!credentials.IsEmpty()) {
    aRv.ThrowTypeError<MSG_URL_HAS_CREDENTIALS>(aInput);
    return;
  }

  nsCOMPtr<nsIURI> resolvedURIClone;
  // We use CloneIgnoringRef to strip away the fragment even if the original URI
  // is immutable.
  aRv = resolvedURI->CloneIgnoringRef(getter_AddRefs(resolvedURIClone));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  nsAutoCString spec;
  aRv = resolvedURIClone->GetSpec(spec);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  CopyUTF8toUTF16(spec, aRequestURL);

  // Get the fragment from nsIURI.
  aRv = resolvedURI->GetRef(aURLfragment);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}
already_AddRefed<nsIURI>
ParseURLFromChrome(const nsAString& aInput, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIURI> uri;
  aRv = NS_NewURI(getter_AddRefs(uri), aInput, nullptr, nullptr);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aInput);
  }
  return uri.forget();
}
void
GetRequestURLFromChrome(const nsAString& aInput, nsAString& aRequestURL,
                        nsACString& aURLfragment, ErrorResult& aRv)
{
  nsCOMPtr<nsIURI> uri = ParseURLFromChrome(aInput, aRv);
  if (aRv.Failed()) {
    return;
  }
  // This fails with URIs with weird protocols, even when they are valid,
  // so we ignore the failure
  nsAutoCString credentials;
  Unused << uri->GetUserPass(credentials);
  if (!credentials.IsEmpty()) {
    aRv.ThrowTypeError<MSG_URL_HAS_CREDENTIALS>(aInput);
    return;
  }

  nsCOMPtr<nsIURI> uriClone;
  // We use CloneIgnoringRef to strip away the fragment even if the original URI
  // is immutable.
  aRv = uri->CloneIgnoringRef(getter_AddRefs(uriClone));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  nsAutoCString spec;
  aRv = uriClone->GetSpec(spec);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  CopyUTF8toUTF16(spec, aRequestURL);

  // Get the fragment from nsIURI.
  aRv = uri->GetRef(aURLfragment);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}
already_AddRefed<URL>
ParseURLFromWorker(const GlobalObject& aGlobal, const nsAString& aInput,
                   ErrorResult& aRv)
{
  workers::WorkerPrivate* worker = workers::GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  NS_ConvertUTF8toUTF16 baseURL(worker->GetLocationInfo().mHref);
  RefPtr<URL> url = URL::WorkerConstructor(aGlobal, aInput, baseURL, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aInput);
  }
  return url.forget();
}
void
GetRequestURLFromWorker(const GlobalObject& aGlobal, const nsAString& aInput,
                        nsAString& aRequestURL, nsACString& aURLfragment,
                        ErrorResult& aRv)
{
  RefPtr<URL> url = ParseURLFromWorker(aGlobal, aInput, aRv);
  if (aRv.Failed()) {
    return;
  }
  nsString username;
  url->GetUsername(username, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsString password;
  url->GetPassword(password, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  if (!username.IsEmpty() || !password.IsEmpty()) {
    aRv.ThrowTypeError<MSG_URL_HAS_CREDENTIALS>(aInput);
    return;
  }
  // Get the fragment from URL.
  nsAutoString fragment;
  url->GetHash(fragment, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // Note: URL::GetHash() includes the "#" and we want the fragment with out
  // the hash symbol.
  if (!fragment.IsEmpty()) {
    CopyUTF16toUTF8(Substring(fragment, 1), aURLfragment);
  }

  url->SetHash(EmptyString(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  url->Stringify(aRequestURL, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

class ReferrerSameOriginChecker final : public workers::WorkerMainThreadRunnable
{
public:
  ReferrerSameOriginChecker(workers::WorkerPrivate* aWorkerPrivate,
                            const nsAString& aReferrerURL,
                            nsresult& aResult)
    : workers::WorkerMainThreadRunnable(aWorkerPrivate,
                                        NS_LITERAL_CSTRING("Fetch :: Referrer same origin check")),
      mReferrerURL(aReferrerURL),
      mResult(aResult)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun() override
  {
    nsCOMPtr<nsIURI> uri;
    if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), mReferrerURL))) {
      nsCOMPtr<nsIPrincipal> principal = mWorkerPrivate->GetPrincipal();
      if (principal) {
        mResult = principal->CheckMayLoad(uri, /* report */ false,
                                          /* allowIfInheritsPrincipal */ false);
      }
    }
    return true;
  }

private:
  const nsString mReferrerURL;
  nsresult& mResult;
};

} // namespace

/*static*/ already_AddRefed<Request>
Request::Constructor(const GlobalObject& aGlobal,
                     const RequestOrUSVString& aInput,
                     const RequestInit& aInit, ErrorResult& aRv)
{
  bool hasCopiedBody = false;
  RefPtr<InternalRequest> request;

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  if (aInput.IsRequest()) {
    RefPtr<Request> inputReq = &aInput.GetAsRequest();
    nsCOMPtr<nsIInputStream> body;
    inputReq->GetBody(getter_AddRefs(body));
    if (inputReq->BodyUsed()) {
      aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
      return nullptr;
    }

    // The body will be copied when GetRequestConstructorCopy() is executed.
    if (body) {
      hasCopiedBody = true;
    }

    request = inputReq->GetInternalRequest();
  } else {
    // aInput is USVString.
    // We need to get url before we create a InternalRequest.
    nsAutoString input;
    input.Assign(aInput.GetAsUSVString());
    nsAutoString requestURL;
    nsCString fragment;
    if (NS_IsMainThread()) {
      nsIDocument* doc = GetEntryDocument();
      if (doc) {
        GetRequestURLFromDocument(doc, input, requestURL, fragment, aRv);
      } else {
        // If we don't have a document, we must assume that this is a full URL.
        GetRequestURLFromChrome(input, requestURL, fragment, aRv);
      }
    } else {
      GetRequestURLFromWorker(aGlobal, input, requestURL, fragment, aRv);
    }
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
    request = new InternalRequest(NS_ConvertUTF16toUTF8(requestURL), fragment);
  }
  request = request->GetRequestConstructorCopy(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  RequestMode fallbackMode = RequestMode::EndGuard_;
  RequestCredentials fallbackCredentials = RequestCredentials::EndGuard_;
  RequestCache fallbackCache = RequestCache::EndGuard_;
  if (aInput.IsUSVString()) {
    fallbackMode = RequestMode::Cors;
    fallbackCredentials = RequestCredentials::Omit;
    fallbackCache = RequestCache::Default;
  }

  RequestMode mode = aInit.mMode.WasPassed() ? aInit.mMode.Value() : fallbackMode;
  RequestCredentials credentials =
    aInit.mCredentials.WasPassed() ? aInit.mCredentials.Value()
                                   : fallbackCredentials;

  if (mode == RequestMode::Navigate ||
      (aInit.IsAnyMemberPresent() && request->Mode() == RequestMode::Navigate)) {
    mode = RequestMode::Same_origin;
  }

  if (aInit.IsAnyMemberPresent()) {
    request->SetReferrer(NS_LITERAL_STRING(kFETCH_CLIENT_REFERRER_STR));
    request->SetReferrerPolicy(ReferrerPolicy::_empty);
  }
  if (aInit.mReferrer.WasPassed()) {
    const nsString& referrer = aInit.mReferrer.Value();
    if (referrer.IsEmpty()) {
      request->SetReferrer(NS_LITERAL_STRING(""));
    } else {
      nsAutoString referrerURL;
      if (NS_IsMainThread()) {
        nsIDocument* doc = GetEntryDocument();
        nsCOMPtr<nsIURI> uri;
        if (doc) {
          uri = ParseURLFromDocument(doc, referrer, aRv);
        } else {
          // If we don't have a document, we must assume that this is a full URL.
          uri = ParseURLFromChrome(referrer, aRv);
        }
        if (NS_WARN_IF(aRv.Failed())) {
          aRv.ThrowTypeError<MSG_INVALID_REFERRER_URL>(referrer);
          return nullptr;
        }
        nsAutoCString spec;
        uri->GetSpec(spec);
        CopyUTF8toUTF16(spec, referrerURL);
        if (!referrerURL.EqualsLiteral(kFETCH_CLIENT_REFERRER_STR)) {
          nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull();
          if (principal) {
            nsresult rv = principal->CheckMayLoad(uri, /* report */ false,
                                                  /* allowIfInheritsPrincipal */ false);
            if (NS_FAILED(rv)) {
              referrerURL.AssignLiteral(kFETCH_CLIENT_REFERRER_STR);
            }
          }
        }
      } else {
        RefPtr<URL> url = ParseURLFromWorker(aGlobal, referrer, aRv);
        if (NS_WARN_IF(aRv.Failed())) {
          aRv.ThrowTypeError<MSG_INVALID_REFERRER_URL>(referrer);
          return nullptr;
        }
        url->Stringify(referrerURL, aRv);
        if (NS_WARN_IF(aRv.Failed())) {
          aRv.ThrowTypeError<MSG_INVALID_REFERRER_URL>(referrer);
          return nullptr;
        }
        if (!referrerURL.EqualsLiteral(kFETCH_CLIENT_REFERRER_STR)) {
          workers::WorkerPrivate* worker = workers::GetCurrentThreadWorkerPrivate();
          nsresult rv = NS_OK;
          // ReferrerSameOriginChecker uses a sync loop to get the main thread
          // to perform the same-origin check.  Overall, on Workers this method
          // can create 3 sync loops (two for constructing URLs and one here) so
          // in the future we may want to optimize it all by off-loading all of
          // this work in a single sync loop.
          RefPtr<ReferrerSameOriginChecker> checker =
            new ReferrerSameOriginChecker(worker, referrerURL, rv);
          IgnoredErrorResult error;
          checker->Dispatch(Terminating, error);
          if (error.Failed() || NS_FAILED(rv)) {
            referrerURL.AssignLiteral(kFETCH_CLIENT_REFERRER_STR);
          }
        }
      }
      request->SetReferrer(referrerURL);
    }
  }

  if (aInit.mReferrerPolicy.WasPassed()) {
    request->SetReferrerPolicy(aInit.mReferrerPolicy.Value());
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
    if (window) {
      nsCOMPtr<nsIDocument> doc;
      doc = window->GetExtantDoc();
      if (doc) {
        request->SetEnvironmentReferrerPolicy(doc->GetReferrerPolicy());
      }
    }
  } else {
    workers::WorkerPrivate* worker = workers::GetCurrentThreadWorkerPrivate();
    if (worker) {
      worker->AssertIsOnWorkerThread();
      request->SetEnvironmentReferrerPolicy(worker->GetReferrerPolicy());
    }
  }

  if (mode != RequestMode::EndGuard_) {
    request->ClearCreatedByFetchEvent();
    request->SetMode(mode);
  }

  if (credentials != RequestCredentials::EndGuard_) {
    request->ClearCreatedByFetchEvent();
    request->SetCredentialsMode(credentials);
  }

  RequestCache cache = aInit.mCache.WasPassed() ?
                       aInit.mCache.Value() : fallbackCache;
  if (cache != RequestCache::EndGuard_) {
    if (cache == RequestCache::Only_if_cached &&
        request->Mode() != RequestMode::Same_origin) {
      uint32_t t = static_cast<uint32_t>(request->Mode());
      NS_ConvertASCIItoUTF16 modeString(RequestModeValues::strings[t].value,
                                        RequestModeValues::strings[t].length);
      aRv.ThrowTypeError<MSG_ONLY_IF_CACHED_WITHOUT_SAME_ORIGIN>(modeString);
      return nullptr;
    }
    request->ClearCreatedByFetchEvent();
    request->SetCacheMode(cache);
  }

  if (aInit.mRedirect.WasPassed()) {
    request->SetRedirectMode(aInit.mRedirect.Value());
  }

  if (aInit.mIntegrity.WasPassed()) {
    request->SetIntegrity(aInit.mIntegrity.Value());
  }

  // Request constructor step 14.
  if (aInit.mMethod.WasPassed()) {
    nsAutoCString method(aInit.mMethod.Value());

    // Step 14.1. Disallow forbidden methods, and anything that is not a HTTP
    // token, since HTTP states that Method may be any of the defined values or
    // a token (extension method).
    nsAutoCString outMethod;
    nsresult rv = FetchUtil::GetValidRequestMethod(method, outMethod);
    if (NS_FAILED(rv)) {
      NS_ConvertUTF8toUTF16 label(method);
      aRv.ThrowTypeError<MSG_INVALID_REQUEST_METHOD>(label);
      return nullptr;
    }

    // Step 14.2
    request->ClearCreatedByFetchEvent();
    request->SetMethod(outMethod);
  }

  RefPtr<InternalHeaders> requestHeaders = request->Headers();

  RefPtr<InternalHeaders> headers;
  if (aInit.mHeaders.WasPassed()) {
    RefPtr<Headers> h = Headers::Constructor(aGlobal, aInit.mHeaders.Value(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    request->ClearCreatedByFetchEvent();
    headers = h->GetInternalHeaders();
  } else {
    headers = new InternalHeaders(*requestHeaders);
  }

  requestHeaders->Clear();
  // From "Let r be a new Request object associated with request and a new
  // Headers object whose guard is "request"."
  requestHeaders->SetGuard(HeadersGuardEnum::Request, aRv);
  MOZ_ASSERT(!aRv.Failed());

  if (request->Mode() == RequestMode::No_cors) {
    if (!request->HasSimpleMethod()) {
      nsAutoCString method;
      request->GetMethod(method);
      NS_ConvertUTF8toUTF16 label(method);
      aRv.ThrowTypeError<MSG_INVALID_REQUEST_METHOD>(label);
      return nullptr;
    }

    if (!request->GetIntegrity().IsEmpty()) {
      aRv.ThrowTypeError<MSG_REQUEST_INTEGRITY_METADATA_NOT_EMPTY>();
      return nullptr;
    }

    requestHeaders->SetGuard(HeadersGuardEnum::Request_no_cors, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  requestHeaders->Fill(*headers, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if ((aInit.mBody.WasPassed() && !aInit.mBody.Value().IsNull()) ||
      hasCopiedBody) {
    // HEAD and GET are not allowed to have a body.
    nsAutoCString method;
    request->GetMethod(method);
    // method is guaranteed to be uppercase due to step 14.2 above.
    if (method.EqualsLiteral("HEAD") || method.EqualsLiteral("GET")) {
      aRv.ThrowTypeError<MSG_NO_BODY_ALLOWED_FOR_GET_AND_HEAD>();
      return nullptr;
    }
  }

  if (aInit.mBody.WasPassed()) {
    const Nullable<fetch::OwningBodyInit>& bodyInitNullable = aInit.mBody.Value();
    if (!bodyInitNullable.IsNull()) {
      const fetch::OwningBodyInit& bodyInit = bodyInitNullable.Value();
      nsCOMPtr<nsIInputStream> stream;
      nsAutoCString contentTypeWithCharset;
      uint64_t contentLengthUnused;
      aRv = ExtractByteStreamFromBody(bodyInit,
                                      getter_AddRefs(stream),
                                      contentTypeWithCharset,
                                      contentLengthUnused);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      nsCOMPtr<nsIInputStream> temporaryBody = stream;

      if (!contentTypeWithCharset.IsVoid() &&
          !requestHeaders->Has(NS_LITERAL_CSTRING("Content-Type"), aRv)) {
        requestHeaders->Append(NS_LITERAL_CSTRING("Content-Type"),
                               contentTypeWithCharset, aRv);
      }

      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      request->ClearCreatedByFetchEvent();

      if (hasCopiedBody) {
        request->SetBody(nullptr);
      }

      request->SetBody(temporaryBody);
    }
  }

  RefPtr<Request> domRequest = new Request(global, request);
  domRequest->SetMimeType();

  if (aInput.IsRequest()) {
    RefPtr<Request> inputReq = &aInput.GetAsRequest();
    nsCOMPtr<nsIInputStream> body;
    inputReq->GetBody(getter_AddRefs(body));
    if (body) {
      inputReq->SetBody(nullptr);
      inputReq->SetBodyUsed();
    }
  }
  return domRequest.forget();
}

already_AddRefed<Request>
Request::Clone(ErrorResult& aRv) const
{
  if (BodyUsed()) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return nullptr;
  }

  RefPtr<InternalRequest> ir = mRequest->Clone();
  if (!ir) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Request> request = new Request(mOwner, ir);
  return request.forget();
}

Headers*
Request::Headers_()
{
  if (!mHeaders) {
    mHeaders = new Headers(mOwner, mRequest->Headers());
  }

  return mHeaders;
}

} // namespace dom
} // namespace mozilla
