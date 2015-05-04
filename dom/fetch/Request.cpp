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
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/URL.h"
#include "mozilla/dom/workers/bindings/URL.h"

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

already_AddRefed<InternalRequest>
Request::GetInternalRequest()
{
  nsRefPtr<InternalRequest> r = mRequest;
  return r.forget();
}

namespace {
void
GetRequestURLFromWindow(const GlobalObject& aGlobal, nsPIDOMWindow* aWindow,
                        const nsAString& aInput, nsAString& aRequestURL,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> docURI = aWindow->GetDocumentURI();
  nsAutoCString spec;
  aRv = docURI->GetSpec(spec);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsRefPtr<dom::URL> url =
    dom::URL::Constructor(aGlobal, aInput, NS_ConvertUTF8toUTF16(spec), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  url->Stringify(aRequestURL, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

void
GetRequestURLFromChrome(const nsAString& aInput, nsAString& aRequestURL,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  nsCOMPtr<nsIURI> uri;
  aRv = NS_NewURI(getter_AddRefs(uri), aInput, nullptr, nullptr);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsAutoCString spec;
  aRv = uri->GetSpec(spec);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  CopyUTF8toUTF16(spec, aRequestURL);
#else
  aRequestURL = aInput;
#endif
}

void
GetRequestURLFromWorker(const GlobalObject& aGlobal, const nsAString& aInput,
                        nsAString& aRequestURL, ErrorResult& aRv)
{
  workers::WorkerPrivate* worker = workers::GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  NS_ConvertUTF8toUTF16 baseURL(worker->GetLocationInfo().mHref);
  nsRefPtr<workers::URL> url =
    workers::URL::Constructor(aGlobal, aInput, baseURL, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  url->Stringify(aRequestURL, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

} // anonymous namespace

/*static*/ already_AddRefed<Request>
Request::Constructor(const GlobalObject& aGlobal,
                     const RequestOrUSVString& aInput,
                     const RequestInit& aInit, ErrorResult& aRv)
{
  nsRefPtr<InternalRequest> request;

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  if (aInput.IsRequest()) {
    nsRefPtr<Request> inputReq = &aInput.GetAsRequest();
    nsCOMPtr<nsIInputStream> body;
    inputReq->GetBody(getter_AddRefs(body));
    if (body) {
      if (inputReq->BodyUsed()) {
        aRv.ThrowTypeError(MSG_FETCH_BODY_CONSUMED_ERROR);
        return nullptr;
      } else {
        inputReq->SetBodyUsed();
      }
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
      nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(global);
      if (window) {
        GetRequestURLFromWindow(aGlobal, window, input, requestURL, aRv);
      } else {
        // If we don't have a window, we must assume that this is a full URL.
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

  // CORS-with-forced-preflight is not publicly exposed and should not be
  // considered a valid value.
  if (aInit.mMode.WasPassed() &&
      aInit.mMode.Value() == RequestMode::Cors_with_forced_preflight) {
    NS_NAMED_LITERAL_STRING(sourceDescription, "'mode' member of RequestInit");
    NS_NAMED_LITERAL_STRING(value, "cors-with-forced-preflight");
    NS_NAMED_LITERAL_STRING(type, "RequestMode");
    aRv.ThrowTypeError(MSG_INVALID_ENUM_VALUE, &sourceDescription, &value, &type);
    return nullptr;
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

  // Request constructor step 14.
  if (aInit.mMethod.WasPassed()) {
    nsAutoCString method(aInit.mMethod.Value());
    nsAutoCString upperCaseMethod = method;
    ToUpperCase(upperCaseMethod);

    // Step 14.1. Disallow forbidden methods, and anything that is not a HTTP
    // token, since HTTP states that Method may be any of the defined values or
    // a token (extension method).
    if (upperCaseMethod.EqualsLiteral("CONNECT") ||
        upperCaseMethod.EqualsLiteral("TRACE") ||
        upperCaseMethod.EqualsLiteral("TRACK") ||
        !NS_IsValidHTTPToken(method)) {
      NS_ConvertUTF8toUTF16 label(method);
      aRv.ThrowTypeError(MSG_INVALID_REQUEST_METHOD, &label);
      return nullptr;
    }

    // Step 14.2
    if (upperCaseMethod.EqualsLiteral("DELETE") ||
        upperCaseMethod.EqualsLiteral("GET") ||
        upperCaseMethod.EqualsLiteral("HEAD") ||
        upperCaseMethod.EqualsLiteral("POST") ||
        upperCaseMethod.EqualsLiteral("PUT") ||
        upperCaseMethod.EqualsLiteral("OPTIONS")) {
      request->ClearCreatedByFetchEvent();
      request->SetMethod(upperCaseMethod);
    } else {
      request->ClearCreatedByFetchEvent();
      request->SetMethod(method);
    }
  }

  nsRefPtr<InternalHeaders> requestHeaders = request->Headers();

  nsRefPtr<InternalHeaders> headers;
  if (aInit.mHeaders.WasPassed()) {
    nsRefPtr<Headers> h = Headers::Constructor(aGlobal, aInit.mHeaders.Value(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    request->ClearCreatedByFetchEvent();
    headers = h->GetInternalHeaders();
  } else {
    headers = new InternalHeaders(*requestHeaders);
  }

  requestHeaders->Clear();

  if (request->Mode() == RequestMode::No_cors) {
    if (!request->HasSimpleMethod()) {
      nsAutoCString method;
      request->GetMethod(method);
      NS_ConvertUTF8toUTF16 label(method);
      aRv.ThrowTypeError(MSG_INVALID_REQUEST_METHOD, &label);
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

  if (aInit.mBody.WasPassed()) {
    // HEAD and GET are not allowed to have a body.
    nsAutoCString method;
    request->GetMethod(method);
    // method is guaranteed to be uppercase due to step 14.2 above.
    if (method.EqualsLiteral("HEAD") || method.EqualsLiteral("GET")) {
      aRv.ThrowTypeError(MSG_NO_BODY_ALLOWED_FOR_GET_AND_HEAD);
      return nullptr;
    }

    const OwningArrayBufferOrArrayBufferViewOrBlobOrFormDataOrUSVStringOrURLSearchParams& bodyInit = aInit.mBody.Value();
    nsCOMPtr<nsIInputStream> stream;
    nsAutoCString contentType;
    aRv = ExtractByteStreamFromBody(bodyInit,
                                    getter_AddRefs(stream), contentType);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
    request->ClearCreatedByFetchEvent();
    request->SetBody(stream);

    if (!contentType.IsVoid() &&
        !requestHeaders->Has(NS_LITERAL_CSTRING("Content-Type"), aRv)) {
      requestHeaders->Append(NS_LITERAL_CSTRING("Content-Type"),
                             contentType, aRv);
    }

    if (aRv.Failed()) {
      return nullptr;
    }
  }

  nsRefPtr<Request> domRequest = new Request(global, request);
  domRequest->SetMimeType();
  return domRequest.forget();
}

already_AddRefed<Request>
Request::Clone(ErrorResult& aRv) const
{
  if (BodyUsed()) {
    aRv.ThrowTypeError(MSG_FETCH_BODY_CONSUMED_ERROR);
    return nullptr;
  }

  nsRefPtr<InternalRequest> ir = mRequest->Clone();
  if (!ir) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Request> request = new Request(mOwner, ir);
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
