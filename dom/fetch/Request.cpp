/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Request, mOwner)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Request)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Request::Request(nsIGlobalObject* aOwner, InternalRequest* aRequest)
  : FetchBody<Request>()
  , mOwner(aOwner)
  , mRequest(aRequest)
{
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

/*static*/ already_AddRefed<Request>
Request::Constructor(const GlobalObject& aGlobal,
                     const RequestOrScalarValueString& aInput,
                     const RequestInit& aInit, ErrorResult& aRv)
{
  nsRefPtr<InternalRequest> request;

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  if (aInput.IsRequest()) {
    nsRefPtr<Request> inputReq = &aInput.GetAsRequest();
    if (inputReq->BodyUsed()) {
      aRv.ThrowTypeError(MSG_REQUEST_BODY_CONSUMED_ERROR);
      return nullptr;
    }

    inputReq->SetBodyUsed();
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
  if (aInput.IsScalarValueString()) {
    nsString input;
    input.Assign(aInput.GetAsScalarValueString());

    nsString requestURL;
    if (NS_IsMainThread()) {
      nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(global);
      MOZ_ASSERT(window);
      nsCOMPtr<nsIURI> docURI = window->GetDocumentURI();
      nsCString spec;
      aRv = docURI->GetSpec(spec);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      nsRefPtr<mozilla::dom::URL> url =
        dom::URL::Constructor(aGlobal, input, NS_ConvertUTF8toUTF16(spec), aRv);
      if (aRv.Failed()) {
        return nullptr;
      }

      url->Stringify(requestURL, aRv);
      if (aRv.Failed()) {
        return nullptr;
      }
    } else {
      workers::WorkerPrivate* worker = workers::GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(worker);
      worker->AssertIsOnWorkerThread();

      nsString baseURL = NS_ConvertUTF8toUTF16(worker->GetLocationInfo().mHref);
      nsRefPtr<workers::URL> url =
        workers::URL::Constructor(aGlobal, input, baseURL, aRv);
      if (aRv.Failed()) {
        return nullptr;
      }

      url->Stringify(requestURL, aRv);
      if (aRv.Failed()) {
        return nullptr;
      }
    }
    request->SetURL(NS_ConvertUTF16toUTF8(requestURL));
    fallbackMode = RequestMode::Cors;
    fallbackCredentials = RequestCredentials::Omit;
  }

  RequestMode mode = aInit.mMode.WasPassed() ? aInit.mMode.Value() : fallbackMode;
  RequestCredentials credentials =
    aInit.mCredentials.WasPassed() ? aInit.mCredentials.Value()
                                   : fallbackCredentials;

  if (mode != RequestMode::EndGuard_) {
    request->SetMode(mode);
  }

  if (credentials != RequestCredentials::EndGuard_) {
    request->SetCredentialsMode(credentials);
  }

  if (aInit.mMethod.WasPassed()) {
    nsCString method = aInit.mMethod.Value();
    ToLowerCase(method);

    if (!method.EqualsASCII("options") &&
        !method.EqualsASCII("get") &&
        !method.EqualsASCII("head") &&
        !method.EqualsASCII("post") &&
        !method.EqualsASCII("put") &&
        !method.EqualsASCII("delete")) {
      NS_ConvertUTF8toUTF16 label(method);
      aRv.ThrowTypeError(MSG_INVALID_REQUEST_METHOD, &label);
      return nullptr;
    }

    ToUpperCase(method);
    request->SetMethod(method);
  }

  nsRefPtr<Request> domRequest = new Request(global, request);
  nsRefPtr<Headers> domRequestHeaders = domRequest->Headers_();

  nsRefPtr<Headers> headers;
  if (aInit.mHeaders.WasPassed()) {
    headers = Headers::Constructor(aGlobal, aInit.mHeaders.Value(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  } else {
    headers = new Headers(*domRequestHeaders);
  }

  domRequestHeaders->Clear();

  if (domRequest->Mode() == RequestMode::No_cors) {
    nsCString method;
    domRequest->GetMethod(method);
    ToLowerCase(method);
    if (!method.EqualsASCII("get") &&
        !method.EqualsASCII("head") &&
        !method.EqualsASCII("post")) {
      NS_ConvertUTF8toUTF16 label(method);
      aRv.ThrowTypeError(MSG_INVALID_REQUEST_METHOD, &label);
      return nullptr;
    }

    domRequestHeaders->SetGuard(HeadersGuardEnum::Request_no_cors, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  domRequestHeaders->Fill(*headers, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (aInit.mBody.WasPassed()) {
    const OwningArrayBufferOrArrayBufferViewOrScalarValueStringOrURLSearchParams& bodyInit = aInit.mBody.Value();
    nsCOMPtr<nsIInputStream> stream;
    nsCString contentType;
    aRv = ExtractByteStreamFromBody(bodyInit,
                                    getter_AddRefs(stream), contentType);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
    request->SetBody(stream);

    if (!contentType.IsVoid() &&
        !domRequestHeaders->Has(NS_LITERAL_CSTRING("Content-Type"), aRv)) {
      domRequestHeaders->Append(NS_LITERAL_CSTRING("Content-Type"),
                                contentType, aRv);
    }

    if (aRv.Failed()) {
      return nullptr;
    }
  }

  domRequest->SetMimeType(aRv);
  return domRequest.forget();
}

already_AddRefed<Request>
Request::Clone() const
{
  // FIXME(nsm): Bug 1073231. This is incorrect, but the clone method isn't
  // well defined yet.
  nsRefPtr<Request> request = new Request(mOwner,
                                          new InternalRequest(*mRequest));
  return request.forget();
}
} // namespace dom
} // namespace mozilla
