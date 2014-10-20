/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"

#include "nsIDocument.h"
#include "nsIGlobalObject.h"
#include "nsIStringStream.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"

#include "nsDOMString.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/FetchDriver.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/workers/Workers.h"

#include "InternalResponse.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

namespace mozilla {
namespace dom {

using namespace workers;

class WorkerFetchResolver MOZ_FINAL : public FetchDriverObserver
{
  friend class WorkerFetchResponseRunnable;
  friend class ResolveFetchWithBodyRunnable;

  // This promise proxy is for the Promise returned by a call to fetch() that
  // is resolved with a Response instance.
  nsRefPtr<PromiseWorkerProxy> mPromiseProxy;
  // Passed from main thread to worker thread after being initialized (except
  // for the body.
  nsRefPtr<InternalResponse> mInternalResponse;
public:

  WorkerFetchResolver(workers::WorkerPrivate* aWorkerPrivate, Promise* aPromise);

  void
  OnResponseAvailable(InternalResponse* aResponse) MOZ_OVERRIDE;

  workers::WorkerPrivate*
  GetWorkerPrivate() { return mPromiseProxy->GetWorkerPrivate(); }

private:
  ~WorkerFetchResolver();
};

class MainThreadFetchResolver MOZ_FINAL : public FetchDriverObserver
{
  nsRefPtr<Promise> mPromise;
  nsRefPtr<InternalResponse> mInternalResponse;

  NS_DECL_OWNINGTHREAD
public:
  MainThreadFetchResolver(Promise* aPromise);

  void
  OnResponseAvailable(InternalResponse* aResponse) MOZ_OVERRIDE;

private:
  ~MainThreadFetchResolver();
};

class MainThreadFetchRunnable : public nsRunnable
{
  nsRefPtr<WorkerFetchResolver> mResolver;
  nsRefPtr<InternalRequest> mRequest;

public:
  MainThreadFetchRunnable(WorkerPrivate* aWorkerPrivate,
                          Promise* aPromise,
                          InternalRequest* aRequest)
    : mResolver(new WorkerFetchResolver(aWorkerPrivate, aPromise))
    , mRequest(aRequest)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_IMETHODIMP
  Run()
  {
    AssertIsOnMainThread();
    nsRefPtr<FetchDriver> fetch = new FetchDriver(mRequest);
    nsresult rv = fetch->Fetch(mResolver);
    // Right now we only support async fetch, which should never directly fail.
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }
};

already_AddRefed<Promise>
FetchRequest(nsIGlobalObject* aGlobal, const RequestOrScalarValueString& aInput,
             const RequestInit& aInit, ErrorResult& aRv)
{
  nsRefPtr<Promise> p = Promise::Create(aGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  AutoJSAPI jsapi;
  jsapi.Init(aGlobal);
  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> jsGlobal(cx, aGlobal->GetGlobalJSObject());
  GlobalObject global(cx, jsGlobal);

  nsRefPtr<Request> request = Request::Constructor(global, aInput, aInit, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<InternalRequest> r = request->GetInternalRequest();
  if (!r->ReferrerIsNone()) {
    nsAutoCString ref;
    aRv = GetRequestReferrer(aGlobal, r, ref);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
    r->SetReferrer(ref);
  }

  if (NS_IsMainThread()) {
    nsRefPtr<MainThreadFetchResolver> resolver = new MainThreadFetchResolver(p);
    nsRefPtr<FetchDriver> fetch = new FetchDriver(r);
    aRv = fetch->Fetch(resolver);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    nsRefPtr<MainThreadFetchRunnable> run = new MainThreadFetchRunnable(worker, p, r);
    if (NS_FAILED(NS_DispatchToMainThread(run))) {
      NS_WARNING("MainThreadFetchRunnable dispatch failed!");
    }
  }

  return p.forget();
}

MainThreadFetchResolver::MainThreadFetchResolver(Promise* aPromise)
  : mPromise(aPromise)
{
}

void
MainThreadFetchResolver::OnResponseAvailable(InternalResponse* aResponse)
{
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
  AssertIsOnMainThread();
  mInternalResponse = aResponse;

  nsCOMPtr<nsIGlobalObject> go = mPromise->GetParentObject();

  nsRefPtr<Response> response = new Response(go, aResponse);
  mPromise->MaybeResolve(response);
}

MainThreadFetchResolver::~MainThreadFetchResolver()
{
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
}

class WorkerFetchResponseRunnable : public WorkerRunnable
{
  nsRefPtr<WorkerFetchResolver> mResolver;
public:
  WorkerFetchResponseRunnable(WorkerFetchResolver* aResolver)
    : WorkerRunnable(aResolver->GetWorkerPrivate(), WorkerThreadModifyBusyCount)
    , mResolver(aResolver)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aWorkerPrivate == mResolver->GetWorkerPrivate());

    nsRefPtr<nsIGlobalObject> global = aWorkerPrivate->GlobalScope();
    nsRefPtr<Response> response = new Response(global, mResolver->mInternalResponse);

    nsRefPtr<Promise> promise = mResolver->mPromiseProxy->GetWorkerPromise();
    MOZ_ASSERT(promise);
    promise->MaybeResolve(response);

    mResolver->mPromiseProxy->CleanUp(aCx);
    return true;
  }
};

WorkerFetchResolver::WorkerFetchResolver(WorkerPrivate* aWorkerPrivate, Promise* aPromise)
  : mPromiseProxy(new PromiseWorkerProxy(aWorkerPrivate, aPromise))
{
}

WorkerFetchResolver::~WorkerFetchResolver()
{
}

void
WorkerFetchResolver::OnResponseAvailable(InternalResponse* aResponse)
{
  AssertIsOnMainThread();
  mInternalResponse = aResponse;

  nsRefPtr<WorkerFetchResponseRunnable> r =
    new WorkerFetchResponseRunnable(this);

  AutoSafeJSContext cx;
  if (!r->Dispatch(cx)) {
    NS_WARNING("Could not dispatch fetch resolve");
  }
}

// Empty string for no-referrer. FIXME(nsm): Does returning empty string
// actually lead to no-referrer in the base channel?
// The actual referrer policy and stripping is dealt with by HttpBaseChannel,
// this always returns the full API referrer URL of the relevant global.
nsresult
GetRequestReferrer(nsIGlobalObject* aGlobal, const InternalRequest* aRequest, nsCString& aReferrer)
{
  if (aRequest->ReferrerIsURL()) {
    aReferrer = aRequest->ReferrerAsURL();
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal);
  if (window) {
    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    if (doc) {
      nsCOMPtr<nsIURI> docURI = doc->GetDocumentURI();
      nsAutoCString origin;
      nsresult rv = nsContentUtils::GetASCIIOrigin(docURI, origin);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsAutoString referrer;
      doc->GetReferrer(referrer);
      aReferrer = NS_ConvertUTF16toUTF8(referrer);
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();
    aReferrer = worker->GetLocationInfo().mHref;
    // XXX(nsm): Algorithm says "If source is not a URL..." but when is it
    // not a URL?
  }

  return NS_OK;
}

namespace {
nsresult
ExtractFromArrayBuffer(const ArrayBuffer& aBuffer,
                       nsIInputStream** aStream)
{
  aBuffer.ComputeLengthAndData();
  //XXXnsm reinterpret_cast<> is used in DOMParser, should be ok.
  return NS_NewByteInputStream(aStream,
                               reinterpret_cast<char*>(aBuffer.Data()),
                               aBuffer.Length(), NS_ASSIGNMENT_COPY);
}

nsresult
ExtractFromArrayBufferView(const ArrayBufferView& aBuffer,
                           nsIInputStream** aStream)
{
  aBuffer.ComputeLengthAndData();
  //XXXnsm reinterpret_cast<> is used in DOMParser, should be ok.
  return NS_NewByteInputStream(aStream,
                               reinterpret_cast<char*>(aBuffer.Data()),
                               aBuffer.Length(), NS_ASSIGNMENT_COPY);
}

nsresult
ExtractFromBlob(const File& aFile, nsIInputStream** aStream,
                nsCString& aContentType)
{
  nsRefPtr<FileImpl> impl = aFile.Impl();
  nsresult rv = impl->GetInternalStream(aStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString type;
  impl->GetType(type);
  aContentType = NS_ConvertUTF16toUTF8(type);
  return NS_OK;
}

nsresult
ExtractFromScalarValueString(const nsString& aStr,
                             nsIInputStream** aStream,
                             nsCString& aContentType)
{
  nsCOMPtr<nsIUnicodeEncoder> encoder = EncodingUtils::EncoderForEncoding("UTF-8");
  if (!encoder) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int32_t destBufferLen;
  nsresult rv = encoder->GetMaxLength(aStr.get(), aStr.Length(), &destBufferLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString encoded;
  if (!encoded.SetCapacity(destBufferLen, fallible_t())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char* destBuffer = encoded.BeginWriting();
  int32_t srcLen = (int32_t) aStr.Length();
  int32_t outLen = destBufferLen;
  rv = encoder->Convert(aStr.get(), &srcLen, destBuffer, &outLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(outLen <= destBufferLen);
  encoded.SetLength(outLen);

  aContentType = NS_LITERAL_CSTRING("text/plain;charset=UTF-8");

  return NS_NewCStringInputStream(aStream, encoded);
}

nsresult
ExtractFromURLSearchParams(const URLSearchParams& aParams,
                           nsIInputStream** aStream,
                           nsCString& aContentType)
{
  nsAutoString serialized;
  aParams.Stringify(serialized);
  aContentType = NS_LITERAL_CSTRING("application/x-www-form-urlencoded;charset=UTF-8");
  return NS_NewStringInputStream(aStream, serialized);
}
}

nsresult
ExtractByteStreamFromBody(const OwningArrayBufferOrArrayBufferViewOrBlobOrScalarValueStringOrURLSearchParams& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType)
{
  MOZ_ASSERT(aStream);

  if (aBodyInit.IsArrayBuffer()) {
    const ArrayBuffer& buf = aBodyInit.GetAsArrayBuffer();
    return ExtractFromArrayBuffer(buf, aStream);
  } else if (aBodyInit.IsArrayBufferView()) {
    const ArrayBufferView& buf = aBodyInit.GetAsArrayBufferView();
    return ExtractFromArrayBufferView(buf, aStream);
  } else if (aBodyInit.IsBlob()) {
    const File& blob = aBodyInit.GetAsBlob();
    return ExtractFromBlob(blob, aStream, aContentType);
  } else if (aBodyInit.IsScalarValueString()) {
    nsAutoString str;
    str.Assign(aBodyInit.GetAsScalarValueString());
    return ExtractFromScalarValueString(str, aStream, aContentType);
  } else if (aBodyInit.IsURLSearchParams()) {
    URLSearchParams& params = aBodyInit.GetAsURLSearchParams();
    return ExtractFromURLSearchParams(params, aStream, aContentType);
  }

  NS_NOTREACHED("Should never reach here");
  return NS_ERROR_FAILURE;
}

nsresult
ExtractByteStreamFromBody(const ArrayBufferOrArrayBufferViewOrBlobOrScalarValueStringOrURLSearchParams& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType)
{
  MOZ_ASSERT(aStream);

  if (aBodyInit.IsArrayBuffer()) {
    const ArrayBuffer& buf = aBodyInit.GetAsArrayBuffer();
    return ExtractFromArrayBuffer(buf, aStream);
  } else if (aBodyInit.IsArrayBufferView()) {
    const ArrayBufferView& buf = aBodyInit.GetAsArrayBufferView();
    return ExtractFromArrayBufferView(buf, aStream);
  } else if (aBodyInit.IsBlob()) {
    const File& blob = aBodyInit.GetAsBlob();
    return ExtractFromBlob(blob, aStream, aContentType);
  } else if (aBodyInit.IsScalarValueString()) {
    nsAutoString str;
    str.Assign(aBodyInit.GetAsScalarValueString());
    return ExtractFromScalarValueString(str, aStream, aContentType);
  } else if (aBodyInit.IsURLSearchParams()) {
    URLSearchParams& params = aBodyInit.GetAsURLSearchParams();
    return ExtractFromURLSearchParams(params, aStream, aContentType);
  }

  NS_NOTREACHED("Should never reach here");
  return NS_ERROR_FAILURE;
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

template <class Derived>
already_AddRefed<Promise>
FetchBody<Derived>::ConsumeBody(ConsumeType aType, ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = Promise::Create(DerivedClass()->GetParentObject(), aRv);
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
  DerivedClass()->GetBody(getter_AddRefs(stream));

  if (!stream) {
    aRv = NS_NewByteInputStream(getter_AddRefs(stream), "", 0,
                                NS_ASSIGNMENT_COPY);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  AutoJSAPI api;
  api.Init(DerivedClass()->GetParentObject());
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
  if (NS_WARN_IF(aRv.Failed())) {
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
      // FIXME(nsm): Use nsContentUtils::CreateBlobBuffer once blobs have been fixed on
      // workers.
      uint32_t blobLen = buffer.Length();
      void* blobData = moz_malloc(blobLen);
      nsRefPtr<File> blob;
      if (blobData) {
        memcpy(blobData, buffer.BeginReading(), blobLen);
        blob = File::CreateMemoryFile(DerivedClass()->GetParentObject(), blobData, blobLen,
                                      NS_ConvertUTF8toUTF16(mMimeType));
      } else {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return nullptr;
      }

      promise->MaybeResolve(blob);
      return promise.forget();
    }
    case CONSUME_JSON: {
      nsAutoString decoded;
      aRv = DecodeUTF8(buffer, decoded);
      if (NS_WARN_IF(aRv.Failed())) {
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
      nsAutoString decoded;
      aRv = DecodeUTF8(buffer, decoded);
      if (NS_WARN_IF(aRv.Failed())) {
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

template
already_AddRefed<Promise>
FetchBody<Request>::ConsumeBody(ConsumeType aType, ErrorResult& aRv);

template
already_AddRefed<Promise>
FetchBody<Response>::ConsumeBody(ConsumeType aType, ErrorResult& aRv);

template <class Derived>
void
FetchBody<Derived>::SetMimeType(ErrorResult& aRv)
{
  // Extract mime type.
  nsTArray<nsCString> contentTypeValues;
  MOZ_ASSERT(DerivedClass()->GetInternalHeaders());
  DerivedClass()->GetInternalHeaders()->GetAll(NS_LITERAL_CSTRING("Content-Type"), contentTypeValues, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // HTTP ABNF states Content-Type may have only one value.
  // This is from the "parse a header value" of the fetch spec.
  if (contentTypeValues.Length() == 1) {
    mMimeType = contentTypeValues[0];
    ToLowerCase(mMimeType);
  }
}

template
void
FetchBody<Request>::SetMimeType(ErrorResult& aRv);

template
void
FetchBody<Response>::SetMimeType(ErrorResult& aRv);
} // namespace dom
} // namespace mozilla
