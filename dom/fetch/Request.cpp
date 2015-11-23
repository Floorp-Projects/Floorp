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
#include "mozilla/dom/workers/bindings/URL.h"
#include "mozilla/unused.h"

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
  : FetchBody<Request>()
  , mOwner(aOwner)
  , mRequest(aRequest)
{
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

// static
bool
Request::RequestCacheEnabled(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.requestcache.enabled", false);
  }

  using namespace workers;

  // Otherwise, check the pref via the WorkerPrivate
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->RequestCacheEnabled();
}

already_AddRefed<InternalRequest>
Request::GetInternalRequest()
{
  RefPtr<InternalRequest> r = mRequest;
  return r.forget();
}

namespace {
void
GetRequestURLFromDocument(nsIDocument* aDocument, const nsAString& aInput,
                          nsAString& aRequestURL, ErrorResult& aRv)
{
  MOZ_ASSERT(aDocument);
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> baseURI = aDocument->GetBaseURI();
  nsCOMPtr<nsIURI> resolvedURI;
  aRv = NS_NewURI(getter_AddRefs(resolvedURI), aInput, nullptr, baseURI);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aInput);
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
}

void
GetRequestURLFromChrome(const nsAString& aInput, nsAString& aRequestURL,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> uri;
  aRv = NS_NewURI(getter_AddRefs(uri), aInput, nullptr, nullptr);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aInput);
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
}

void
GetRequestURLFromWorker(const GlobalObject& aGlobal, const nsAString& aInput,
                        nsAString& aRequestURL, ErrorResult& aRv)
{
  workers::WorkerPrivate* worker = workers::GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  NS_ConvertUTF8toUTF16 baseURL(worker->GetLocationInfo().mHref);
  RefPtr<workers::URL> url =
    workers::URL::Constructor(aGlobal, aInput, baseURL, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aInput);
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

  url->SetHash(EmptyString(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  url->Stringify(aRequestURL, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

} // namespace

/*static*/ already_AddRefed<Request>
Request::Constructor(const GlobalObject& aGlobal,
                     const RequestOrUSVString& aInput,
                     const RequestInit& aInit, ErrorResult& aRv)
{
  nsCOMPtr<nsIInputStream> temporaryBody;
  RefPtr<InternalRequest> request;

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  if (aInput.IsRequest()) {
    RefPtr<Request> inputReq = &aInput.GetAsRequest();
    nsCOMPtr<nsIInputStream> body;
    inputReq->GetBody(getter_AddRefs(body));
    if (body) {
      if (inputReq->BodyUsed()) {
        aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
        return nullptr;
      }
      temporaryBody = body;
    }

    request = inputReq->GetInternalRequest();
  } else {
    request = new InternalRequest();
  }

  request = request->GetRequestConstructorCopy(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RequestMode fallbackMode = RequestMode::EndGuard_;
  RequestCredentials fallbackCredentials = RequestCredentials::EndGuard_;
  RequestCache fallbackCache = RequestCache::EndGuard_;
  if (aInput.IsUSVString()) {
    nsAutoString input;
    input.Assign(aInput.GetAsUSVString());

    nsAutoString requestURL;
    if (NS_IsMainThread()) {
      nsIDocument* doc = GetEntryDocument();
      if (doc) {
        GetRequestURLFromDocument(doc, input, requestURL, aRv);
      } else {
        // If we don't have a document, we must assume that this is a full URL.
        GetRequestURLFromChrome(input, requestURL, aRv);
      }
    } else {
      GetRequestURLFromWorker(aGlobal, input, requestURL, aRv);
    }

    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    request->SetURL(NS_ConvertUTF16toUTF8(requestURL));
    fallbackMode = RequestMode::Cors;
    fallbackCredentials = RequestCredentials::Omit;
    fallbackCache = RequestCache::Default;
  }

  RequestMode mode = aInit.mMode.WasPassed() ? aInit.mMode.Value() : fallbackMode;
  RequestCredentials credentials =
    aInit.mCredentials.WasPassed() ? aInit.mCredentials.Value()
                                   : fallbackCredentials;

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
    request->ClearCreatedByFetchEvent();
    request->SetCacheMode(cache);
  }

  if (aInit.mRedirect.WasPassed()) {
    request->SetRedirectMode(aInit.mRedirect.Value());
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

    requestHeaders->SetGuard(HeadersGuardEnum::Request_no_cors, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  requestHeaders->Fill(*headers, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (aInit.mBody.WasPassed() || temporaryBody) {
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
    const OwningArrayBufferOrArrayBufferViewOrBlobOrFormDataOrUSVStringOrURLSearchParams& bodyInit = aInit.mBody.Value();
    nsCOMPtr<nsIInputStream> stream;
    nsAutoCString contentType;
    aRv = ExtractByteStreamFromBody(bodyInit,
                                    getter_AddRefs(stream), contentType);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    temporaryBody = stream;

    if (!contentType.IsVoid() &&
        !requestHeaders->Has(NS_LITERAL_CSTRING("Content-Type"), aRv)) {
      requestHeaders->Append(NS_LITERAL_CSTRING("Content-Type"),
                             contentType, aRv);
    }

    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    request->ClearCreatedByFetchEvent();
    request->SetBody(temporaryBody);
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
