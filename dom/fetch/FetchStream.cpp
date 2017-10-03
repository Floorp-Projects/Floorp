/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchStream.h"
#include "mozilla/dom/DOMException.h"
#include "nsITransport.h"
#include "nsIStreamTransportService.h"
#include "nsProxyRelease.h"
#include "WorkerPrivate.h"
#include "Workers.h"

#define FETCH_STREAM_FLAG 0

static NS_DEFINE_CID(kStreamTransportServiceCID,
                     NS_STREAMTRANSPORTSERVICE_CID);

namespace mozilla {
namespace dom {

using namespace workers;

namespace {

class FetchStreamWorkerHolder final : public WorkerHolder
{
public:
  explicit FetchStreamWorkerHolder(FetchStream* aStream)
    : WorkerHolder(WorkerHolder::Behavior::AllowIdleShutdownStart)
    , mStream(aStream)
    , mWasNotified(false)
  {}

  bool Notify(Status aStatus) override
  {
    if (!mWasNotified) {
      mWasNotified = true;
      mStream->Close();
    }

    return true;
  }

  WorkerPrivate* GetWorkerPrivate() const
  {
    return mWorkerPrivate;
  }

private:
  RefPtr<FetchStream> mStream;
  bool mWasNotified;
};

class FetchStreamWorkerHolderShutdown final : public WorkerControlRunnable
{
public:
  FetchStreamWorkerHolderShutdown(WorkerPrivate* aWorkerPrivate,
                                  UniquePtr<WorkerHolder>&& aHolder,
                                  nsCOMPtr<nsIGlobalObject>&& aGlobal,
                                  RefPtr<FetchStreamHolder>&& aStreamHolder)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , mHolder(Move(aHolder))
    , mGlobal(Move(aGlobal))
    , mStreamHolder(Move(aStreamHolder))
  {}

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mHolder = nullptr;
    mGlobal = nullptr;

    mStreamHolder->NullifyStream();
    mStreamHolder = nullptr;

    return true;
  }

  // This runnable starts from a JS Thread. We need to disable a couple of
  // assertions overring the following methods.

  bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    return true;
  }

  void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {}

private:
  UniquePtr<WorkerHolder> mHolder;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<FetchStreamHolder> mStreamHolder;
};

} // anonymous

NS_IMPL_ISUPPORTS(FetchStream, nsIInputStreamCallback, nsIObserver,
                  nsISupportsWeakReference)

/* static */ void
FetchStream::Create(JSContext* aCx, FetchStreamHolder* aStreamHolder,
                    nsIGlobalObject* aGlobal, nsIInputStream* aInputStream,
                    JS::MutableHandle<JSObject*> aStream, ErrorResult& aRv)
{
  MOZ_DIAGNOSTIC_ASSERT(aCx);
  MOZ_DIAGNOSTIC_ASSERT(aStreamHolder);
  MOZ_DIAGNOSTIC_ASSERT(aInputStream);

  RefPtr<FetchStream> stream =
    new FetchStream(aGlobal, aStreamHolder, aInputStream);

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (NS_WARN_IF(!os)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    aRv = os->AddObserver(stream, DOM_WINDOW_DESTROYED_TOPIC, true);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

  } else {
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    MOZ_ASSERT(workerPrivate);

    UniquePtr<FetchStreamWorkerHolder> holder(
      new FetchStreamWorkerHolder(stream));
    if (NS_WARN_IF(!holder->HoldWorker(workerPrivate, Closing))) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    // Note, this will create a ref-cycle between the holder and the stream.
    // The cycle is broken when the stream is closed or the worker begins
    // shutting down.
    stream->mWorkerHolder = Move(holder);
  }

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
    return;
  }

  // This will be released in FetchStream::FinalizeCallback().  We are
  // guaranteed the jsapi will call FinalizeCallback when ReadableStream
  // js object is finalized.
  NS_ADDREF(stream.get());

  aStream.set(body);
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
                        stream->mState == eChecking ||
                        stream->mState == eReading);

  if (stream->mState == eReading) {
    // We are already reading data.
    return;
  }

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
                                    stream->mOwningEventTarget);
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
    stream->CloseAndReleaseObjects(aCx, aStream);
    return;
  }

  rv = stream->mInputStream->AsyncWait(stream, 0, 0,
                                       stream->mOwningEventTarget);
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

  // This is safe because we created an extra reference in FetchStream::Create()
  // that won't be released until FetchStream::FinalizeCallback() is called.
  // We are guaranteed that won't happen until the js ReadableStream object
  // is finalized.
  FetchStream* stream = static_cast<FetchStream*>(aUnderlyingSource);

  if (stream->mInputStream) {
    stream->mInputStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
  }

  stream->ReleaseObjects();
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

  // This is safe because we created an extra reference in FetchStream::Create()
  // that won't be released until FetchStream::FinalizeCallback() is called.
  // We are guaranteed that won't happen until the js ReadableStream object
  // is finalized.
  FetchStream* stream = static_cast<FetchStream*>(aUnderlyingSource);

  if (stream->mInputStream) {
    stream->mInputStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
  }

  stream->ReleaseObjects();
}

void
FetchStream::FinalizeCallback(void* aUnderlyingSource, uint8_t aFlags)
{
  MOZ_DIAGNOSTIC_ASSERT(aUnderlyingSource);
  MOZ_DIAGNOSTIC_ASSERT(aFlags == FETCH_STREAM_FLAG);

  // This can be called in any thread.

  // This takes ownership of the ref created in FetchStream::Create().
  RefPtr<FetchStream> stream =
    dont_AddRef(static_cast<FetchStream*>(aUnderlyingSource));

  stream->ReleaseObjects();
}

FetchStream::FetchStream(nsIGlobalObject* aGlobal,
                         FetchStreamHolder* aStreamHolder,
                         nsIInputStream* aInputStream)
  : mState(eWaiting)
  , mGlobal(aGlobal)
  , mStreamHolder(aStreamHolder)
  , mOwningEventTarget(aGlobal->EventTargetFor(TaskCategory::Other))
  , mOriginalInputStream(aInputStream)
{
  MOZ_DIAGNOSTIC_ASSERT(aInputStream);
  MOZ_DIAGNOSTIC_ASSERT(aStreamHolder);
}

FetchStream::~FetchStream()
{
}

void
FetchStream::ErrorPropagation(JSContext* aCx, JS::HandleObject aStream,
                              nsresult aError)
{
  // Nothing to do.
  if (mState == eClosed) {
    return;
  }

  // Let's close the stream.
  if (aError == NS_BASE_STREAM_CLOSED) {
    CloseAndReleaseObjects(aCx, aStream);
    return;
  }

  // Let's use a generic error.
  RefPtr<DOMException> error = DOMException::Create(NS_ERROR_DOM_TYPE_ERR);

  JS::Rooted<JS::Value> errorValue(aCx);
  if (ToJSValue(aCx, error, &errorValue)) {
    JS::ReadableStreamError(aCx, aStream, errorValue);
  }

  ReleaseObjects();
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
  JS::Rooted<JSObject*> stream(cx, mStreamHolder->ReadableStreamBody());

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

/* static */ nsresult
FetchStream::RetrieveInputStream(void* aUnderlyingReadableStreamSource,
                                 nsIInputStream** aInputStream)
{
  MOZ_ASSERT(aUnderlyingReadableStreamSource);
  MOZ_ASSERT(aInputStream);

  RefPtr<FetchStream> stream =
    static_cast<FetchStream*>(aUnderlyingReadableStreamSource);

  // if mOriginalInputStream is null, the reading already started. We don't want
  // to expose the internal inputStream.
  if (NS_WARN_IF(!stream->mOriginalInputStream)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCOMPtr<nsIInputStream> inputStream = stream->mOriginalInputStream;
  inputStream.forget(aInputStream);
  return NS_OK;
}

void
FetchStream::Close()
{
  if (mState == eClosed) {
    return;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    ReleaseObjects();
    return;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> stream(cx, mStreamHolder->ReadableStreamBody());
  CloseAndReleaseObjects(cx, stream);
}

void
FetchStream::CloseAndReleaseObjects(JSContext* aCx, JS::HandleObject aStream)
{
  MOZ_DIAGNOSTIC_ASSERT(mState != eClosed);

  ReleaseObjects();

  if (JS::ReadableStreamIsReadable(aStream)) {
    JS::ReadableStreamClose(aCx, aStream);
  }
}

void
FetchStream::ReleaseObjects()
{
  if (mState == eClosed) {
    return;
  }

  mState = eClosed;

  if (mWorkerHolder) {
    RefPtr<FetchStreamWorkerHolderShutdown> r =
      new FetchStreamWorkerHolderShutdown(
        static_cast<FetchStreamWorkerHolder*>(mWorkerHolder.get())->GetWorkerPrivate(),
        Move(mWorkerHolder), Move(mGlobal), Move(mStreamHolder));
    r->Dispatch();
  } else {
    RefPtr<FetchStream> self = this;
    RefPtr<Runnable> r = NS_NewRunnableFunction(
      "FetchStream::ReleaseObjects",
      [self] () {
        nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
        if (obs) {
          obs->RemoveObserver(self, DOM_WINDOW_DESTROYED_TOPIC);
        }
        self->mGlobal = nullptr;

        self->mStreamHolder->NullifyStream();
        self->mStreamHolder = nullptr;
      });

    mOwningEventTarget->Dispatch(r.forget());
  }
}

// nsIObserver
// -----------

NS_IMETHODIMP
FetchStream::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData)
{
  AssertIsOnMainThread();

  MOZ_ASSERT(strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (SameCOMIdentity(aSubject, window)) {
    Close();
  }

  return NS_OK;
}

} // dom namespace
} // mozilla namespace
