/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Request.h"

#include "nsIUnicodeDecoder.h"
#include "nsIURI.h"

#include "nsDOMFile.h"
#include "nsDOMString.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/URL.h"
#include "mozilla/dom/workers/bindings/URL.h"

// dom/workers
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
  : mOwner(aOwner)
  , mRequest(aRequest)
  , mBodyUsed(false)
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

  // Extract mime type.
  nsTArray<nsCString> contentTypeValues;
  domRequestHeaders->GetAll(NS_LITERAL_CSTRING("Content-Type"),
                            contentTypeValues, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // HTTP ABNF states Content-Type may have only one value.
  // This is from the "parse a header value" of the fetch spec.
  if (contentTypeValues.Length() == 1) {
    domRequest->mMimeType = contentTypeValues[0];
    ToLowerCase(domRequest->mMimeType);
  }

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

namespace {
nsresult
DecodeUTF8(const nsCString& aBuffer, nsString& aDecoded)
{
  nsCOMPtr<nsIUnicodeDecoder> decoder =
    EncodingUtils::DecoderForEncoding("UTF-8");
  if (!decoder) {
    return NS_ERROR_FAILURE;
  }

  int32_t destBufferLen;
  nsresult rv =
    decoder->GetMaxLength(aBuffer.get(), aBuffer.Length(), &destBufferLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!aDecoded.SetCapacity(destBufferLen, fallible_t())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char16_t* destBuffer = aDecoded.BeginWriting();
  int32_t srcLen = (int32_t) aBuffer.Length();
  int32_t outLen = destBufferLen;
  rv = decoder->Convert(aBuffer.get(), &srcLen, destBuffer, &outLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(outLen <= destBufferLen);
  aDecoded.SetLength(outLen);
  return NS_OK;
}
}

already_AddRefed<Promise>
Request::ConsumeBody(ConsumeType aType, ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = Promise::Create(mOwner, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (BodyUsed()) {
    aRv.ThrowTypeError(MSG_REQUEST_BODY_CONSUMED_ERROR);
    return nullptr;
  }

  SetBodyUsed();

  // While the spec says to do this asynchronously, all the body constructors
  // right now only accept bodies whose streams are backed by an in-memory
  // buffer that can be read without blocking. So I think this is fine.
  nsCOMPtr<nsIInputStream> stream;
  mRequest->GetBody(getter_AddRefs(stream));

  if (!stream) {
    aRv = NS_NewByteInputStream(getter_AddRefs(stream), "", 0,
                                NS_ASSIGNMENT_COPY);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  AutoJSAPI api;
  api.Init(mOwner);
  JSContext* cx = api.cx();

  // We can make this assertion because for now we only support memory backed
  // structures for the body argument for a Request.
  MOZ_ASSERT(NS_InputStreamIsBuffered(stream));
  nsCString buffer;
  uint64_t len;
  aRv = stream->Available(&len);
  if (aRv.Failed()) {
    return nullptr;
  }

  aRv = NS_ReadInputStreamToString(stream, buffer, len);
  if (aRv.Failed()) {
    return nullptr;
  }

  buffer.SetLength(len);

  switch (aType) {
    case CONSUME_ARRAYBUFFER: {
      JS::Rooted<JSObject*> arrayBuffer(cx);
      arrayBuffer =
        ArrayBuffer::Create(cx, buffer.Length(),
                            reinterpret_cast<const uint8_t*>(buffer.get()));
      JS::Rooted<JS::Value> val(cx);
      val.setObjectOrNull(arrayBuffer);
      promise->MaybeResolve(cx, val);
      return promise.forget();
    }
    case CONSUME_BLOB: {
      // XXXnsm it is actually possible to avoid these duplicate allocations
      // for the Blob case by having the Blob adopt the stream's memory
      // directly, but I've not added a special case for now.
      //
      // This is similar to nsContentUtils::CreateBlobBuffer, but also deals
      // with worker wrapping.
      uint32_t blobLen = buffer.Length();
      void* blobData = moz_malloc(blobLen);
      nsRefPtr<DOMFile> blob;
      if (blobData) {
        memcpy(blobData, buffer.BeginReading(), blobLen);
        blob = DOMFile::CreateMemoryFile(GetParentObject(), blobData, blobLen,
                                         NS_ConvertUTF8toUTF16(mMimeType));
      } else {
        aRv = NS_ERROR_OUT_OF_MEMORY;
        return nullptr;
      }

      promise->MaybeResolve(blob);
      return promise.forget();
    }
    case CONSUME_JSON: {
      nsString decoded;
      aRv = DecodeUTF8(buffer, decoded);
      if (aRv.Failed()) {
        return nullptr;
      }

      JS::Rooted<JS::Value> json(cx);
      if (!JS_ParseJSON(cx, decoded.get(), decoded.Length(), &json)) {
        JS::Rooted<JS::Value> exn(cx);
        if (JS_GetPendingException(cx, &exn)) {
          JS_ClearPendingException(cx);
          promise->MaybeReject(cx, exn);
        }
      }
      promise->MaybeResolve(cx, json);
      return promise.forget();
    }
    case CONSUME_TEXT: {
      nsString decoded;
      aRv = DecodeUTF8(buffer, decoded);
      if (aRv.Failed()) {
        return nullptr;
      }

      promise->MaybeResolve(decoded);
      return promise.forget();
    }
  }

  NS_NOTREACHED("Unexpected consume body type");
  // Silence warnings.
  return nullptr;
}

already_AddRefed<Promise>
Request::ArrayBuffer(ErrorResult& aRv)
{
  return ConsumeBody(CONSUME_ARRAYBUFFER, aRv);
}

already_AddRefed<Promise>
Request::Blob(ErrorResult& aRv)
{
  return ConsumeBody(CONSUME_BLOB, aRv);
}

already_AddRefed<Promise>
Request::Json(ErrorResult& aRv)
{
  return ConsumeBody(CONSUME_JSON, aRv);
}

already_AddRefed<Promise>
Request::Text(ErrorResult& aRv)
{
  return ConsumeBody(CONSUME_TEXT, aRv);
}
} // namespace dom
} // namespace mozilla
