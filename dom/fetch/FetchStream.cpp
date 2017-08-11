/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchStream.h"
#include "nsITransport.h"
#include "nsIStreamTransportService.h"
#include "nsProxyRelease.h"

#define FETCH_STREAM_FLAG 0

static NS_DEFINE_CID(kStreamTransportServiceCID,
                     NS_STREAMTRANSPORTSERVICE_CID);

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(FetchStream, nsIInputStreamCallback)

/* static */ JSObject*
FetchStream::Create(JSContext* aCx, nsIGlobalObject* aGlobal,
                    nsIInputStream* aInputStream, ErrorResult& aRv)
{
  MOZ_DIAGNOSTIC_ASSERT(aCx);
  MOZ_DIAGNOSTIC_ASSERT(aInputStream);

  RefPtr<FetchStream> stream = new FetchStream(aGlobal, aInputStream);

  if (!JS::HasReadableStreamCallbacks(aCx)) {
    JS::SetReadableStreamCallbacks(aCx,
                                   &FetchStream::RequestDataCallback,
                                   &FetchStream::WriteIntoReadRequestCallback,
                                   &FetchStream::CancelCallback,
                                   &FetchStream::ClosedCallback,
                                   &FetchStream::ErroredCallback,
                                   &FetchStream::FinalizeCallback);
  }

  JS::Rooted<JSObject*> body(aCx,
    JS::NewReadableExternalSourceStreamObject(aCx, stream, FETCH_STREAM_FLAG));
  if (!body) {
    aRv.StealExceptionFromJSContext(aCx);
    return nullptr;
  }

  stream->mReadableStream = body;

  // JS engine will call the finalize callback.
  NS_ADDREF(stream.get());
  return body;
}

/* static */ void
FetchStream::RequestDataCallback(JSContext* aCx,
                                 JS::HandleObject aStream,
                                 void* aUnderlyingSource,
                                 uint8_t aFlags,
                                 size_t aDesiredSize)
{
  MOZ_DIAGNOSTIC_ASSERT(aUnderlyingSource);
  MOZ_DIAGNOSTIC_ASSERT(aFlags == FETCH_STREAM_FLAG);
  MOZ_DIAGNOSTIC_ASSERT(JS::ReadableStreamIsDisturbed(aStream));

  RefPtr<FetchStream> stream = static_cast<FetchStream*>(aUnderlyingSource);

  MOZ_DIAGNOSTIC_ASSERT(stream->mState == eWaiting ||
                        stream->mState == eChecking);

  if (stream->mState == eChecking) {
    // If we are looking for more data, there is nothing else we should do:
    // let's move this checking operation in a reading.
    MOZ_ASSERT(stream->mInputStream);
    stream->mState = eReading;
    return;
  }

  stream->mState = eReading;

  if (!stream->mInputStream) {
    // This is the first use of the stream. Let's convert the
    // mOriginalInputStream into an nsIAsyncInputStream.
    MOZ_ASSERT(stream->mOriginalInputStream);

    bool nonBlocking = false;
    nsresult rv = stream->mOriginalInputStream->IsNonBlocking(&nonBlocking);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      stream->ErrorPropagation(aCx, aStream, rv);
      return;
    }

    nsCOMPtr<nsIAsyncInputStream> asyncStream =
      do_QueryInterface(stream->mOriginalInputStream);
    if (!nonBlocking || !asyncStream) {
      nsCOMPtr<nsIStreamTransportService> sts =
        do_GetService(kStreamTransportServiceCID, &rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        stream->ErrorPropagation(aCx, aStream, rv);
        return;
      }

      nsCOMPtr<nsITransport> transport;
      rv = sts->CreateInputTransport(stream->mOriginalInputStream,
                                     /* aStartOffset */ 0,
                                     /* aReadLimit */ -1,
                                     /* aCloseWhenDone */ true,
                                     getter_AddRefs(transport));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        stream->ErrorPropagation(aCx, aStream, rv);
        return;
      }

      nsCOMPtr<nsIInputStream> wrapper;
      rv = transport->OpenInputStream(/* aFlags */ 0,
                                       /* aSegmentSize */ 0,
                                       /* aSegmentCount */ 0,
                                       getter_AddRefs(wrapper));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        stream->ErrorPropagation(aCx, aStream, rv);
        return;
      }

      asyncStream = do_QueryInterface(wrapper);
    }

    stream->mInputStream = asyncStream;
    stream->mOriginalInputStream = nullptr;
  }

  MOZ_DIAGNOSTIC_ASSERT(stream->mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(!stream->mOriginalInputStream);

  nsresult rv =
    stream->mInputStream->AsyncWait(stream, 0, 0,
                                    stream->mGlobal->EventTargetFor(TaskCategory::Other));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    stream->ErrorPropagation(aCx, aStream, rv);
    return;
  }

  // All good.
}

/* static */ void
FetchStream::WriteIntoReadRequestCallback(JSContext* aCx,
                                          JS::HandleObject aStream,
                                          void* aUnderlyingSource,
                                          uint8_t aFlags, void* aBuffer,
                                          size_t aLength, size_t* aByteWritten)
{
  MOZ_DIAGNOSTIC_ASSERT(aUnderlyingSource);
  MOZ_DIAGNOSTIC_ASSERT(aFlags == FETCH_STREAM_FLAG);
  MOZ_DIAGNOSTIC_ASSERT(aBuffer);
  MOZ_DIAGNOSTIC_ASSERT(aByteWritten);

  RefPtr<FetchStream> stream = static_cast<FetchStream*>(aUnderlyingSource);

  MOZ_DIAGNOSTIC_ASSERT(stream->mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(stream->mState == eWriting);
  stream->mState = eChecking;

  uint32_t written;
  nsresult rv =
    stream->mInputStream->Read(static_cast<char*>(aBuffer), aLength, &written);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    stream->ErrorPropagation(aCx, aStream, rv);
    return;
  }

  *aByteWritten = written;

  if (written == 0) {
    stream->mState = eClosed;
    JS::ReadableStreamClose(aCx, aStream);
    return;
  }

  rv = stream->mInputStream->AsyncWait(stream, 0, 0,
                                       stream->mGlobal->EventTargetFor(TaskCategory::Other));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    stream->ErrorPropagation(aCx, aStream, rv);
    return;
  }

  // All good.
}

/* static */ JS::Value
FetchStream::CancelCallback(JSContext* aCx, JS::HandleObject aStream,
                            void* aUnderlyingSource, uint8_t aFlags,
                            JS::HandleValue aReason)
{
  MOZ_DIAGNOSTIC_ASSERT(aUnderlyingSource);
  MOZ_DIAGNOSTIC_ASSERT(aFlags == FETCH_STREAM_FLAG);

  RefPtr<FetchStream> stream = static_cast<FetchStream*>(aUnderlyingSource);

  if (stream->mInputStream) {
    stream->mInputStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
  }

  stream->mState = eClosed;

  return JS::UndefinedValue();
}

/* static */ void
FetchStream::ClosedCallback(JSContext* aCx, JS::HandleObject aStream,
                            void* aUnderlyingSource, uint8_t aFlags)
{
  MOZ_DIAGNOSTIC_ASSERT(aUnderlyingSource);
  MOZ_DIAGNOSTIC_ASSERT(aFlags == FETCH_STREAM_FLAG);
}

/* static */ void
FetchStream::ErroredCallback(JSContext* aCx, JS::HandleObject aStream,
                             void* aUnderlyingSource, uint8_t aFlags,
                             JS::HandleValue aReason)
{
  MOZ_DIAGNOSTIC_ASSERT(aUnderlyingSource);
  MOZ_DIAGNOSTIC_ASSERT(aFlags == FETCH_STREAM_FLAG);
}

void
FetchStream::FinalizeCallback(void* aUnderlyingSource, uint8_t aFlags)
{
  MOZ_DIAGNOSTIC_ASSERT(aUnderlyingSource);
  MOZ_DIAGNOSTIC_ASSERT(aFlags == FETCH_STREAM_FLAG);

  RefPtr<FetchStream> stream =
    dont_AddRef(static_cast<FetchStream*>(aUnderlyingSource));

  stream->mState = eClosed;
  stream->mReadableStream = nullptr;
}

FetchStream::FetchStream(nsIGlobalObject* aGlobal,
                         nsIInputStream* aInputStream)
  : mState(eWaiting)
  , mGlobal(aGlobal)
  , mOriginalInputStream(aInputStream)
  , mOwningEventTarget(mGlobal->EventTargetFor(TaskCategory::Other))
  , mReadableStream(nullptr)
{
  MOZ_DIAGNOSTIC_ASSERT(aInputStream);
}

FetchStream::~FetchStream()
{
  NS_ProxyRelease("FetchStream::mGlobal", mOwningEventTarget, mGlobal.forget());
}

void
FetchStream::ErrorPropagation(JSContext* aCx, JS::HandleObject aStream,
                              nsresult aError)
{
  // Nothing to do.
  if (mState == eClosed) {
    return;
  }

  // We cannot continue with any other operation.
  mState = eClosed;

  // Let's close the stream.
  if (aError == NS_BASE_STREAM_CLOSED) {
    JS::ReadableStreamClose(aCx, aStream);
    return;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);

  // Let's use a generic error.
  RefPtr<DOMError> error = new DOMError(window, NS_ERROR_DOM_TYPE_ERR);

  JS::Rooted<JS::Value> errorValue(aCx);
  if (ToJSValue(aCx, error, &errorValue)) {
    JS::ReadableStreamError(aCx, aStream, errorValue);
  }
}

NS_IMETHODIMP
FetchStream::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  MOZ_DIAGNOSTIC_ASSERT(aStream);

  // Already closed. We have nothing else to do here.
  if (mState == eClosed) {
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(mState == eReading || mState == eChecking);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    // Without JSContext we are not able to close the stream or to propagate the
    // error.
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> stream(cx, mReadableStream);

  uint64_t size = 0;
  nsresult rv = mInputStream->Available(&size);
  if (NS_SUCCEEDED(rv) && size == 0) {
    // In theory this should not happen. If size is 0, the stream should be
    // considered closed.
    rv = NS_BASE_STREAM_CLOSED;
  }

  // No warning for stream closed.
  if (rv == NS_BASE_STREAM_CLOSED || NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(cx, stream, rv);
    return NS_OK;
  }

  // This extra checking is completed. Let's wait for the next read request.
  if (mState == eChecking) {
    mState = eWaiting;
    return NS_OK;
  }

  mState = eWriting;
  JS::ReadableStreamUpdateDataAvailableFromSource(cx, stream, size);

  // The WriteInto callback changes mState to eChecking.
  MOZ_DIAGNOSTIC_ASSERT(mState == eChecking);

  return NS_OK;
}

} // dom namespace
} // mozilla namespace
