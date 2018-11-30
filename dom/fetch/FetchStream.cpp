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

static NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);

namespace mozilla {
namespace dom {

class FetchStream::WorkerShutdown final : public WorkerControlRunnable {
 public:
  WorkerShutdown(WorkerPrivate* aWorkerPrivate, RefPtr<FetchStream> aStream)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mStream(aStream) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mStream->ReleaseObjects();
    return true;
  }

  // This runnable starts from a JS Thread. We need to disable a couple of
  // assertions overring the following methods.

  bool PreDispatch(WorkerPrivate* aWorkerPrivate) override { return true; }

  void PostDispatch(WorkerPrivate* aWorkerPrivate,
                    bool aDispatchResult) override {}

 private:
  RefPtr<FetchStream> mStream;
};

NS_IMPL_ISUPPORTS(FetchStream, nsIInputStreamCallback, nsIObserver,
                  nsISupportsWeakReference)

/* static */ void FetchStream::Create(JSContext* aCx,
                                      FetchStreamHolder* aStreamHolder,
                                      nsIGlobalObject* aGlobal,
                                      nsIInputStream* aInputStream,
                                      JS::MutableHandle<JSObject*> aStream,
                                      ErrorResult& aRv) {
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
        WeakWorkerRef::Create(workerPrivate, [stream]() { stream->Close(); });

    if (NS_WARN_IF(!workerRef)) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    // Note, this will create a ref-cycle between the holder and the stream.
    // The cycle is broken when the stream is closed or the worker begins
    // shutting down.
    stream->mWorkerRef = workerRef.forget();
  }

  aRv.MightThrowJSException();
  JS::Rooted<JSObject*> body(
      aCx, JS::NewReadableExternalSourceStreamObject(aCx, stream));
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

void FetchStream::requestData(JSContext* aCx, JS::HandleObject aStream,
                              size_t aDesiredSize) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool disturbed;
  if (!JS::ReadableStreamIsDisturbed(aCx, aStream, &disturbed)) {
    JS_ClearPendingException(aCx);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(disturbed);
  }
#endif

  AssertIsOnOwningThread();

  MutexAutoLock lock(mMutex);

  MOZ_DIAGNOSTIC_ASSERT(mState == eInitializing || mState == eWaiting ||
                        mState == eChecking || mState == eReading);

  if (mState == eReading) {
    // We are already reading data.
    return;
  }

  if (mState == eChecking) {
    // If we are looking for more data, there is nothing else we should do:
    // let's move this checking operation in a reading.
    MOZ_ASSERT(mInputStream);
    mState = eReading;
    return;
  }

  if (mState == eInitializing) {
    // The stream has been used for the first time.
    mStreamHolder->MarkAsRead();
  }

  mState = eReading;

  if (!mInputStream) {
    // This is the first use of the stream. Let's convert the
    // mOriginalInputStream into an nsIAsyncInputStream.
    MOZ_ASSERT(mOriginalInputStream);

    nsCOMPtr<nsIAsyncInputStream> asyncStream;
    nsresult rv = NS_MakeAsyncNonBlockingInputStream(
        mOriginalInputStream.forget(), getter_AddRefs(asyncStream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ErrorPropagation(aCx, lock, aStream, rv);
      return;
    }

    mInputStream = asyncStream;
    mOriginalInputStream = nullptr;
  }

  MOZ_DIAGNOSTIC_ASSERT(mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(!mOriginalInputStream);

  nsresult rv = mInputStream->AsyncWait(this, 0, 0, mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(aCx, lock, aStream, rv);
    return;
  }

  // All good.
}

void FetchStream::writeIntoReadRequestBuffer(JSContext* aCx,
                                             JS::HandleObject aStream,
                                             void* aBuffer, size_t aLength,
                                             size_t* aByteWritten) {
  MOZ_DIAGNOSTIC_ASSERT(aBuffer);
  MOZ_DIAGNOSTIC_ASSERT(aByteWritten);

  AssertIsOnOwningThread();

  MutexAutoLock lock(mMutex);

  MOZ_DIAGNOSTIC_ASSERT(mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(mState == eWriting);
  mState = eChecking;

  uint32_t written;
  nsresult rv =
      mInputStream->Read(static_cast<char*>(aBuffer), aLength, &written);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(aCx, lock, aStream, rv);
    return;
  }

  *aByteWritten = written;

  if (written == 0) {
    CloseAndReleaseObjects(aCx, lock, aStream);
    return;
  }

  rv = mInputStream->AsyncWait(this, 0, 0, mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(aCx, lock, aStream, rv);
    return;
  }

  // All good.
}

JS::Value FetchStream::cancel(JSContext* aCx, JS::HandleObject aStream,
                              JS::HandleValue aReason) {
  AssertIsOnOwningThread();

  if (mState == eInitializing) {
    // The stream has been used for the first time.
    mStreamHolder->MarkAsRead();
  }

  if (mInputStream) {
    mInputStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
  }

  // It could be that we don't have mInputStream yet, but we still have the
  // original stream. We need to close that too.
  if (mOriginalInputStream) {
    MOZ_ASSERT(!mInputStream);
    mOriginalInputStream->Close();
  }

  ReleaseObjects();
  return JS::UndefinedValue();
}

void FetchStream::onClosed(JSContext* aCx, JS::HandleObject aStream) {}

void FetchStream::onErrored(JSContext* aCx, JS::HandleObject aStream,
                            JS::HandleValue aReason) {
  AssertIsOnOwningThread();

  if (mState == eInitializing) {
    // The stream has been used for the first time.
    mStreamHolder->MarkAsRead();
  }

  if (mInputStream) {
    mInputStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
  }

  ReleaseObjects();
}

void FetchStream::finalize() {
  // This can be called in any thread.

  // This takes ownership of the ref created in FetchStream::Create().
  RefPtr<FetchStream> stream = dont_AddRef(this);

  stream->ReleaseObjects();
}

FetchStream::FetchStream(nsIGlobalObject* aGlobal,
                         FetchStreamHolder* aStreamHolder,
                         nsIInputStream* aInputStream)
    : mMutex("FetchStream::mMutex"),
      mState(eInitializing),
      mGlobal(aGlobal),
      mStreamHolder(aStreamHolder),
      mOwningEventTarget(aGlobal->EventTargetFor(TaskCategory::Other)),
      mOriginalInputStream(aInputStream) {
  MOZ_DIAGNOSTIC_ASSERT(aInputStream);
  MOZ_DIAGNOSTIC_ASSERT(aStreamHolder);
}

FetchStream::~FetchStream() {}

void FetchStream::ErrorPropagation(JSContext* aCx,
                                   const MutexAutoLock& aProofOfLock,
                                   JS::HandleObject aStream, nsresult aError) {
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
FetchStream::OnInputStreamReady(nsIAsyncInputStream* aStream) {
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

/* static */ nsresult FetchStream::RetrieveInputStream(
    JS::ReadableStreamUnderlyingSource* aUnderlyingReadableStreamSource,
    nsIInputStream** aInputStream) {
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

void FetchStream::Close() {
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

void FetchStream::CloseAndReleaseObjects(JSContext* aCx,
                                         const MutexAutoLock& aProofOfLock,
                                         JS::HandleObject aStream) {
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(mState != eClosed);

  ReleaseObjects(aProofOfLock);

  MutexAutoUnlock unlock(mMutex);
  bool readable;
  if (!JS::ReadableStreamIsReadable(aCx, aStream, &readable)) {
    return;
  }
  if (readable) {
    JS::ReadableStreamClose(aCx, aStream);
  }
}

void FetchStream::ReleaseObjects() {
  MutexAutoLock lock(mMutex);
  ReleaseObjects(lock);
}

void FetchStream::ReleaseObjects(const MutexAutoLock& aProofOfLock) {
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
    RefPtr<Runnable> r = NS_NewRunnableFunction(
        "FetchStream::ReleaseObjects", [self]() { self->ReleaseObjects(); });
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
void FetchStream::AssertIsOnOwningThread() {
  NS_ASSERT_OWNINGTHREAD(FetchStream);
}
#endif

// nsIObserver
// -----------

NS_IMETHODIMP
FetchStream::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) {
  AssertIsOnMainThread();
  AssertIsOnOwningThread();

  MOZ_ASSERT(strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (SameCOMIdentity(aSubject, window)) {
    Close();
  }

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
