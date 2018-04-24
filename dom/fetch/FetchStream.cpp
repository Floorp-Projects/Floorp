/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchStream.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsProxyRelease.h"
#include "nsStreamUtils.h"

#define FETCH_STREAM_FLAG 0

static NS_DEFINE_CID(kStreamTransportServiceCID,
                     NS_STREAMTRANSPORTSERVICE_CID);

namespace mozilla {
namespace dom {

class FetchStream::WorkerShutdown final : public WorkerControlRunnable
{
public:
  WorkerShutdown(WorkerPrivate* aWorkerPrivate, RefPtr<FetchStream> aStream)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , mStream(aStream)
  {}

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mStream->ReleaseObjects();
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
  RefPtr<FetchStream> mStream;
};

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

    RefPtr<WeakWorkerRef> workerRef =
      WeakWorkerRef::Create(workerPrivate, [stream]() {
        stream->Close();
      });

    if (NS_WARN_IF(!workerRef)) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    // Note, this will create a ref-cycle between the holder and the stream.
    // The cycle is broken when the stream is closed or the worker begins
    // shutting down.
    stream->mWorkerRef = workerRef.forget();
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
  stream->AssertIsOnOwningThread();

  MutexAutoLock lock(stream->mMutex);

  MOZ_DIAGNOSTIC_ASSERT(stream->mState == eInitializing ||
                        stream->mState == eWaiting ||
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

  if (stream->mState == eInitializing) {
    // The stream has been used for the first time.
    stream->mStreamHolder->MarkAsRead();
  }

  stream->mState = eReading;

  if (!stream->mInputStream) {
    // This is the first use of the stream. Let's convert the
    // mOriginalInputStream into an nsIAsyncInputStream.
    MOZ_ASSERT(stream->mOriginalInputStream);

    nsCOMPtr<nsIAsyncInputStream> asyncStream;
    nsresult rv =
      NS_MakeAsyncNonBlockingInputStream(stream->mOriginalInputStream.forget(),
                                         getter_AddRefs(asyncStream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      stream->ErrorPropagation(aCx, lock, aStream, rv);
      return;
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
    stream->ErrorPropagation(aCx, lock, aStream, rv);
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
  stream->AssertIsOnOwningThread();

  MutexAutoLock lock(stream->mMutex);

  MOZ_DIAGNOSTIC_ASSERT(stream->mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(stream->mState == eWriting);
  stream->mState = eChecking;

  uint32_t written;
  nsresult rv =
    stream->mInputStream->Read(static_cast<char*>(aBuffer), aLength, &written);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    stream->ErrorPropagation(aCx, lock, aStream, rv);
    return;
  }

  *aByteWritten = written;

  if (written == 0) {
    stream->CloseAndReleaseObjects(aCx, lock, aStream);
    return;
  }

  rv = stream->mInputStream->AsyncWait(stream, 0, 0,
                                       stream->mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    stream->ErrorPropagation(aCx, lock, aStream, rv);
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
  stream->AssertIsOnOwningThread();

  if (stream->mState == eInitializing) {
    // The stream has been used for the first time.
    stream->mStreamHolder->MarkAsRead();
  }

  if (stream->mInputStream) {
    stream->mInputStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
  }

  // It could be that we don't have mInputStream yet, but we still have the
  // original stream. We need to close that too.
  if (stream->mOriginalInputStream) {
    MOZ_ASSERT(!stream->mInputStream);
    stream->mOriginalInputStream->Close();
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
  stream->AssertIsOnOwningThread();

  if (stream->mState == eInitializing) {
    // The stream has been used for the first time.
    stream->mStreamHolder->MarkAsRead();
  }

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
  : mMutex("FetchStream::mMutex")
  , mState(eInitializing)
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
FetchStream::ErrorPropagation(JSContext* aCx,
                              const MutexAutoLock& aProofOfLock,
                              JS::HandleObject aStream,
                              nsresult aError)
{
  AssertIsOnOwningThread();

  // Nothing to do.
  if (mState == eClosed) {
    return;
  }

  // Let's close the stream.
  if (aError == NS_BASE_STREAM_CLOSED) {
    CloseAndReleaseObjects(aCx, aProofOfLock, aStream);
    return;
  }

  // Let's use a generic error.
  RefPtr<DOMException> error = DOMException::Create(NS_ERROR_DOM_TYPE_ERR);

  JS::Rooted<JS::Value> errorValue(aCx);
  if (ToJSValue(aCx, error, &errorValue)) {
    MutexAutoUnlock unlock(mMutex);
    JS::ReadableStreamError(aCx, aStream, errorValue);
  }

  ReleaseObjects(aProofOfLock);
}

NS_IMETHODIMP
FetchStream::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(aStream);

  MutexAutoLock lock(mMutex);

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
    ErrorPropagation(cx, lock, stream, rv);
    return NS_OK;
  }

  // This extra checking is completed. Let's wait for the next read request.
  if (mState == eChecking) {
    mState = eWaiting;
    return NS_OK;
  }

  mState = eWriting;

  {
    MutexAutoUnlock unlock(mMutex);
    JS::ReadableStreamUpdateDataAvailableFromSource(cx, stream, size);
  }

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
  stream->AssertIsOnOwningThread();

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
  AssertIsOnOwningThread();

  MutexAutoLock lock(mMutex);

  if (mState == eClosed) {
    return;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    ReleaseObjects(lock);
    return;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> stream(cx, mStreamHolder->ReadableStreamBody());
  CloseAndReleaseObjects(cx, lock, stream);
}

void
FetchStream::CloseAndReleaseObjects(JSContext* aCx,
                                    const MutexAutoLock& aProofOfLock,
                                    JS::HandleObject aStream)
{
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(mState != eClosed);

  ReleaseObjects(aProofOfLock);

  MutexAutoUnlock unlock(mMutex);
  if (JS::ReadableStreamIsReadable(aStream)) {
    JS::ReadableStreamClose(aCx, aStream);
  }
}

void
FetchStream::ReleaseObjects()
{
  MutexAutoLock lock(mMutex);
  ReleaseObjects(lock);
}

void
FetchStream::ReleaseObjects(const MutexAutoLock& aProofOfLock)
{
  // This method can be called on 2 possible threads: the owning one and a JS
  // thread used to release resources. If we are on the JS thread, we need to
  // dispatch a runnable to go back to the owning thread in order to release
  // resources correctly.

  if (mState == eClosed) {
    // Already gone. Nothing to do.
    return;
  }

  mState = eClosed;

  if (!NS_IsMainThread() && !IsCurrentThreadRunningWorker()) {
    // Let's dispatch a WorkerControlRunnable if the owning thread is a worker.
    if (mWorkerRef) {
      RefPtr<WorkerShutdown> r =
        new WorkerShutdown(mWorkerRef->GetUnsafePrivate(), this);
      Unused << NS_WARN_IF(!r->Dispatch());
      return;
    }

    // A normal runnable of the owning thread is the main-thread.
    RefPtr<FetchStream> self = this;
    RefPtr<Runnable> r =
      NS_NewRunnableFunction("FetchStream::ReleaseObjects",
                             [self] () { self->ReleaseObjects(); });
    mOwningEventTarget->Dispatch(r.forget());
    return;
  }

  AssertIsOnOwningThread();

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
    }
  }

  mWorkerRef = nullptr;
  mGlobal = nullptr;

  mStreamHolder->NullifyStream();
  mStreamHolder = nullptr;
}

#ifdef DEBUG
void
FetchStream::AssertIsOnOwningThread()
{
  NS_ASSERT_OWNINGTHREAD(FetchStream);
}
#endif

// nsIObserver
// -----------

NS_IMETHODIMP
FetchStream::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData)
{
  AssertIsOnMainThread();
  AssertIsOnOwningThread();

  MOZ_ASSERT(strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (SameCOMIdentity(aSubject, window)) {
    Close();
  }

  return NS_OK;
}

} // dom namespace
} // mozilla namespace
