/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Response.h"

#include "nsISupportsImpl.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FetchBinding.h"
#include "mozilla/dom/ResponseBinding.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/URL.h"

#include "nsDOMString.h"

#include "BodyExtractor.h"
#include "FetchStream.h"
#include "FetchStreamReader.h"
#include "InternalResponse.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(Response)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Response)

NS_IMPL_CYCLE_COLLECTION_CLASS(Response)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Response)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHeaders)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSignal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFetchStreamReader)

  tmp->mReadableStreamBody = nullptr;
  tmp->mReadableStreamReader = nullptr;

  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Response)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHeaders)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSignal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFetchStreamReader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Response)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mReadableStreamBody)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mReadableStreamReader)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Response)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Response::Response(nsIGlobalObject* aGlobal,
                   InternalResponse* aInternalResponse,
                   AbortSignal* aSignal)
  : FetchBody<Response>(aGlobal)
  , mInternalResponse(aInternalResponse)
  , mSignal(aSignal)
{
  MOZ_ASSERT(aInternalResponse->Headers()->Guard() == HeadersGuardEnum::Immutable ||
             aInternalResponse->Headers()->Guard() == HeadersGuardEnum::Response);
  SetMimeType();

  mozilla::HoldJSObjects(this);
}

Response::~Response()
{
  mozilla::DropJSObjects(this);
}

/* static */ already_AddRefed<Response>
Response::Error(const GlobalObject& aGlobal)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<InternalResponse> error = InternalResponse::NetworkError(NS_ERROR_FAILURE);
  RefPtr<Response> r = new Response(global, error, nullptr);
  return r.forget();
}

/* static */ already_AddRefed<Response>
Response::Redirect(const GlobalObject& aGlobal, const nsAString& aUrl,
                   uint16_t aStatus, ErrorResult& aRv)
{
  nsAutoString parsedURL;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIURI> baseURI;
    nsIDocument* doc = GetEntryDocument();
    if (doc) {
      baseURI = doc->GetBaseURI();
    }
    nsCOMPtr<nsIURI> resolvedURI;
    aRv = NS_NewURI(getter_AddRefs(resolvedURI), aUrl, nullptr, baseURI);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    nsAutoCString spec;
    aRv = resolvedURI->GetSpec(spec);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    CopyUTF8toUTF16(spec, parsedURL);
  } else {
    workers::WorkerPrivate* worker = workers::GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();

    NS_ConvertUTF8toUTF16 baseURL(worker->GetLocationInfo().mHref);
    RefPtr<URL> url = URL::WorkerConstructor(aGlobal, aUrl, baseURL, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    url->Stringify(parsedURL, aRv);
  }

  if (aRv.Failed()) {
    return nullptr;
  }

  if (aStatus != 301 && aStatus != 302 && aStatus != 303 && aStatus != 307 && aStatus != 308) {
    aRv.ThrowRangeError<MSG_INVALID_REDIRECT_STATUSCODE_ERROR>();
    return nullptr;
  }

  Optional<Nullable<fetch::ResponseBodyInit>> body;
  ResponseInit init;
  init.mStatus = aStatus;
  RefPtr<Response> r = Response::Constructor(aGlobal, body, init, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  r->GetInternalHeaders()->Set(NS_LITERAL_CSTRING("Location"),
                               NS_ConvertUTF16toUTF8(parsedURL), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  r->GetInternalHeaders()->SetGuard(HeadersGuardEnum::Immutable, aRv);
  MOZ_ASSERT(!aRv.Failed());

  return r.forget();
}

/*static*/ already_AddRefed<Response>
Response::Constructor(const GlobalObject& aGlobal,
                      const Optional<Nullable<fetch::ResponseBodyInit>>& aBody,
                      const ResponseInit& aInit, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  if (aInit.mStatus < 200 || aInit.mStatus > 599) {
    aRv.ThrowRangeError<MSG_INVALID_RESPONSE_STATUSCODE_ERROR>();
    return nullptr;
  }

  // Check if the status text contains illegal characters
  nsACString::const_iterator start, end;
  aInit.mStatusText.BeginReading(start);
  aInit.mStatusText.EndReading(end);
  if (FindCharInReadable('\r', start, end)) {
    aRv.ThrowTypeError<MSG_RESPONSE_INVALID_STATUSTEXT_ERROR>();
    return nullptr;
  }
  // Reset iterator since FindCharInReadable advances it.
  aInit.mStatusText.BeginReading(start);
  if (FindCharInReadable('\n', start, end)) {
    aRv.ThrowTypeError<MSG_RESPONSE_INVALID_STATUSTEXT_ERROR>();
    return nullptr;
  }

  RefPtr<InternalResponse> internalResponse =
    new InternalResponse(aInit.mStatus, aInit.mStatusText);

  // Grab a valid channel info from the global so this response is 'valid' for
  // interception.
  if (NS_IsMainThread()) {
    ChannelInfo info;
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
    if (window) {
      nsIDocument* doc = window->GetExtantDoc();
      MOZ_ASSERT(doc);
      info.InitFromDocument(doc);
    } else {
      info.InitFromChromeGlobal(global);
    }
    internalResponse->InitChannelInfo(info);
  } else {
    workers::WorkerPrivate* worker = workers::GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    internalResponse->InitChannelInfo(worker->GetChannelInfo());
  }

  RefPtr<Response> r = new Response(global, internalResponse, nullptr);

  if (aInit.mHeaders.WasPassed()) {
    internalResponse->Headers()->Clear();

    // Instead of using Fill, create an object to allow the constructor to
    // unwrap the HeadersInit.
    RefPtr<Headers> headers =
      Headers::Create(global, aInit.mHeaders.Value(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    internalResponse->Headers()->Fill(*headers->GetInternalHeaders(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  if (aBody.WasPassed() && !aBody.Value().IsNull()) {
    if (aInit.mStatus == 204 || aInit.mStatus == 205 || aInit.mStatus == 304) {
      aRv.ThrowTypeError<MSG_RESPONSE_NULL_STATUS_WITH_BODY>();
      return nullptr;
    }

    nsCString contentTypeWithCharset;
    nsCOMPtr<nsIInputStream> bodyStream;
    int64_t bodySize = InternalResponse::UNKNOWN_BODY_SIZE;

    const fetch::ResponseBodyInit& body = aBody.Value().Value();
    if (body.IsReadableStream()) {
      const ReadableStream& readableStream = body.GetAsReadableStream();

      JS::Rooted<JSObject*> readableStreamObj(aGlobal.Context(),
                                              readableStream.Obj());

      if (JS::ReadableStreamIsDisturbed(readableStreamObj) ||
          JS::ReadableStreamIsLocked(readableStreamObj) ||
          !JS::ReadableStreamIsReadable(readableStreamObj)) {
        aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
        return nullptr;
      }

      r->SetReadableStreamBody(aGlobal.Context(), readableStreamObj);

      if (JS::ReadableStreamGetMode(readableStreamObj) ==
            JS::ReadableStreamMode::ExternalSource) {
        // If this is a DOM generated ReadableStream, we can extract the
        // inputStream directly.
        void* underlyingSource = nullptr;
        if (!JS::ReadableStreamGetExternalUnderlyingSource(aGlobal.Context(),
                                                           readableStreamObj,
                                                           &underlyingSource)) {
          aRv.StealExceptionFromJSContext(aGlobal.Context());
          return nullptr;
        }

        MOZ_ASSERT(underlyingSource);

        aRv = FetchStream::RetrieveInputStream(underlyingSource,
                                               getter_AddRefs(bodyStream));

        // The releasing of the external source is needed in order to avoid an
        // extra stream lock.
        JS::ReadableStreamReleaseExternalUnderlyingSource(readableStreamObj);
        if (NS_WARN_IF(aRv.Failed())) {
          return nullptr;
        }
      } else {
        // If this is a JS-created ReadableStream, let's create a
        // FetchStreamReader.
        aRv = FetchStreamReader::Create(aGlobal.Context(), global,
                                        getter_AddRefs(r->mFetchStreamReader),
                                        getter_AddRefs(bodyStream));
        if (NS_WARN_IF(aRv.Failed())) {
          return nullptr;
        }
      }
    } else {
      uint64_t size = 0;
      aRv = ExtractByteStreamFromBody(body,
                                      getter_AddRefs(bodyStream),
                                      contentTypeWithCharset,
                                      size);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      bodySize = size;
    }

    internalResponse->SetBody(bodyStream, bodySize);

    if (!contentTypeWithCharset.IsVoid() &&
        !internalResponse->Headers()->Has(NS_LITERAL_CSTRING("Content-Type"),
                                          aRv)) {
      // Ignore Append() failing here.
      ErrorResult error;
      internalResponse->Headers()->Append(NS_LITERAL_CSTRING("Content-Type"),
                                          contentTypeWithCharset, error);
      error.SuppressException();
    }

    if (aRv.Failed()) {
      return nullptr;
    }
  }

  r->SetMimeType();
  return r.forget();
}

already_AddRefed<Response>
Response::Clone(JSContext* aCx, ErrorResult& aRv)
{
  if (BodyUsed()) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return nullptr;
  }

  RefPtr<FetchStreamReader> streamReader;
  nsCOMPtr<nsIInputStream> inputStream;

  JS::Rooted<JSObject*> body(aCx);
  MaybeTeeReadableStreamBody(aCx, &body,
                             getter_AddRefs(streamReader),
                             getter_AddRefs(inputStream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  MOZ_ASSERT_IF(body, streamReader);
  MOZ_ASSERT_IF(body, inputStream);

  RefPtr<InternalResponse> ir =
    mInternalResponse->Clone(body
      ? InternalResponse::eDontCloneInputStream
      : InternalResponse::eCloneInputStream);

  RefPtr<Response> response = new Response(mOwner, ir, mSignal);

  if (body) {
    // Maybe we have a body, but we receive null from MaybeTeeReadableStreamBody
    // if this body is a native stream.   In this case the InternalResponse will
    // have a clone of the native body and the ReadableStream will be created
    // lazily if needed.
    response->SetReadableStreamBody(aCx, body);
    response->mFetchStreamReader = streamReader;
    ir->SetBody(inputStream, InternalResponse::UNKNOWN_BODY_SIZE);
  }

  return response.forget();
}

already_AddRefed<Response>
Response::CloneUnfiltered(JSContext* aCx, ErrorResult& aRv)
{
  if (BodyUsed()) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return nullptr;
  }

  RefPtr<FetchStreamReader> streamReader;
  nsCOMPtr<nsIInputStream> inputStream;

  JS::Rooted<JSObject*> body(aCx);
  MaybeTeeReadableStreamBody(aCx, &body,
                             getter_AddRefs(streamReader),
                             getter_AddRefs(inputStream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  MOZ_ASSERT_IF(body, streamReader);
  MOZ_ASSERT_IF(body, inputStream);

  RefPtr<InternalResponse> clone =
    mInternalResponse->Clone(body
      ? InternalResponse::eDontCloneInputStream
      : InternalResponse::eCloneInputStream);

  RefPtr<InternalResponse> ir = clone->Unfiltered();
  RefPtr<Response> ref = new Response(mOwner, ir, mSignal);

  if (body) {
    // Maybe we have a body, but we receive null from MaybeTeeReadableStreamBody
    // if this body is a native stream.   In this case the InternalResponse will
    // have a clone of the native body and the ReadableStream will be created
    // lazily if needed.
    ref->SetReadableStreamBody(aCx, body);
    ref->mFetchStreamReader = streamReader;
    ir->SetBody(inputStream, InternalResponse::UNKNOWN_BODY_SIZE);
  }

  return ref.forget();
}

void
Response::SetBody(nsIInputStream* aBody, int64_t aBodySize)
{
  MOZ_ASSERT(!BodyUsed());
  mInternalResponse->SetBody(aBody, aBodySize);
}

already_AddRefed<InternalResponse>
Response::GetInternalResponse() const
{
  RefPtr<InternalResponse> ref = mInternalResponse;
  return ref.forget();
}

Headers*
Response::Headers_()
{
  if (!mHeaders) {
    mHeaders = new Headers(mOwner, mInternalResponse->Headers());
  }

  return mHeaders;
}

} // namespace dom
} // namespace mozilla
