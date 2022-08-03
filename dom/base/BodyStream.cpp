/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BodyStream.h"
#include "js/GCAPI.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableByteStreamController.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsIObserverService.h"
#include "nsProxyRelease.h"
#include "nsStreamUtils.h"

#include <cstdint>

namespace mozilla::dom {

// BodyStreamHolder
// ---------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(BodyStreamHolder)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BodyStreamHolder)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BodyStreamHolder)
  if (tmp->mBodyStream) {
    tmp->mBodyStream->ReleaseObjects();
    MOZ_ASSERT(!tmp->mBodyStream);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(BodyStreamHolder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BodyStreamHolder)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BodyStreamHolder)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BodyStreamHolder::BodyStreamHolder() : mBodyStream(nullptr) {}

void BodyStreamHolder::StoreBodyStream(BodyStream* aBodyStream) {
  MOZ_ASSERT(aBodyStream);
  MOZ_ASSERT(!mBodyStream);
  mBodyStream = aBodyStream;
}

// BodyStream
// ---------------------------------------------------------------------------

class BodyStream::WorkerShutdown final : public WorkerControlRunnable {
 public:
  WorkerShutdown(WorkerPrivate* aWorkerPrivate, RefPtr<BodyStream> aStream)
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
  RefPtr<BodyStream> mStream;
};

NS_IMPL_ISUPPORTS(BodyStream, nsIInputStreamCallback, nsIObserver,
                  nsISupportsWeakReference)

/* static */
void BodyStream::Create(JSContext* aCx, BodyStreamHolder* aStreamHolder,
                        nsIGlobalObject* aGlobal, nsIInputStream* aInputStream,
                        ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(aCx);
  MOZ_DIAGNOSTIC_ASSERT(aStreamHolder);
  MOZ_DIAGNOSTIC_ASSERT(aInputStream);

  RefPtr<BodyStream> stream =
      new BodyStream(aGlobal, aStreamHolder, aInputStream);

  auto cleanup = MakeScopeExit([stream] { stream->Close(); });

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

    RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
        workerPrivate, "BodyStream", [stream]() { stream->Close(); });

    if (NS_WARN_IF(!workerRef)) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    // Note, this will create a ref-cycle between the holder and the stream.
    // The cycle is broken when the stream is closed or the worker begins
    // shutting down.
    stream->mWorkerRef = std::move(workerRef);
  }

  RefPtr<ReadableStream> body =
      ReadableStream::Create(aCx, aGlobal, aStreamHolder, aRv);
  if (aRv.Failed()) {
    return;
  }

  cleanup.release();

  aStreamHolder->StoreBodyStream(stream);
  aStreamHolder->SetReadableStreamBody(body);

#ifdef DEBUG
  aStreamHolder->mStreamCreated = true;
#endif
}

// UnderlyingSource.pull, implemented for BodyStream.
already_AddRefed<Promise> BodyStream::PullCallback(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  MOZ_ASSERT(aController.IsByte());
  ReadableStream* stream = aController.AsByte()->Stream();
  MOZ_ASSERT(stream);

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  MOZ_DIAGNOSTIC_ASSERT(stream->Disturbed());
#endif

  AssertIsOnOwningThread();

  MutexSingleWriterAutoLock lock(mMutex);

  MOZ_DIAGNOSTIC_ASSERT(mState == eInitializing || mState == eWaiting ||
                        mState == eChecking || mState == eReading);

  RefPtr<Promise> resolvedWithUndefinedPromise =
      Promise::CreateResolvedWithUndefined(aController.GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mState == eReading) {
    // We are already reading data.
    return resolvedWithUndefinedPromise.forget();
  }

  if (mState == eChecking) {
    // If we are looking for more data, there is nothing else we should do:
    // let's move this checking operation in a reading.
    MOZ_ASSERT(mInputStream);
    mState = eReading;

    return resolvedWithUndefinedPromise.forget();
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
      ErrorPropagation(aCx, lock, stream, rv);
      return nullptr;
    }

    mInputStream = asyncStream;
    mOriginalInputStream = nullptr;
  }

  MOZ_DIAGNOSTIC_ASSERT(mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(!mOriginalInputStream);

  nsresult rv = mInputStream->AsyncWait(this, 0, 0, mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(aCx, lock, stream, rv);
    return nullptr;
  }
  mAsyncWaitWorkerRef = mWorkerRef;

  // All good.
  return resolvedWithUndefinedPromise.forget();
}

void BodyStream::WriteIntoReadRequestBuffer(JSContext* aCx,
                                            ReadableStream* aStream,
                                            JS::Handle<JSObject*> aChunk,
                                            uint32_t aLength,
                                            uint32_t* aByteWritten) {
  MOZ_DIAGNOSTIC_ASSERT(aChunk);
  MOZ_DIAGNOSTIC_ASSERT(aByteWritten);

  AssertIsOnOwningThread();

  MutexSingleWriterAutoLock lock(mMutex);

  MOZ_DIAGNOSTIC_ASSERT(mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(mState == eWriting);
  mState = eChecking;

  uint32_t written;
  nsresult rv;
  void* buffer;
  {
    // Bug 1754513: Hazard suppression.
    //
    // Because mInputStream->Read is detected as possibly GCing by the
    // current state of our static hazard analysis, we need to do the
    // suppression here. This can be removed with future improvements
    // to the static analysis.
    JS::AutoSuppressGCAnalysis suppress;
    JS::AutoCheckCannotGC noGC;
    bool isSharedMemory;

    buffer = JS_GetArrayBufferViewData(aChunk, &isSharedMemory, noGC);
    MOZ_ASSERT(!isSharedMemory);

    rv = mInputStream->Read(static_cast<char*>(buffer), aLength, &written);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ErrorPropagation(aCx, lock, aStream, rv);
      return;
    }
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
  mAsyncWaitWorkerRef = mWorkerRef;

  // All good.
}

// UnderlyingSource.cancel callback, implmented for BodyStream.
already_AddRefed<Promise> BodyStream::CancelCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  mMutex.AssertOnWritingThread();

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

  RefPtr<Promise> promise = Promise::CreateResolvedWithUndefined(mGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Must come after all uses of members!
  ReleaseObjects();

  return promise.forget();
}

// Non-standard UnderlyingSource.error callback.
void BodyStream::ErrorCallback() {
  mMutex.AssertOnWritingThread();

  if (mState == eInitializing) {
    // The stream has been used for the first time.
    mStreamHolder->MarkAsRead();
  }

  if (mInputStream) {
    mInputStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
  }

  ReleaseObjects();
}

BodyStream::BodyStream(nsIGlobalObject* aGlobal,
                       BodyStreamHolder* aStreamHolder,
                       nsIInputStream* aInputStream)
    : mMutex("BodyStream::mMutex", this),
      mState(eInitializing),
      mGlobal(aGlobal),
      mStreamHolder(aStreamHolder),
      mOwningEventTarget(aGlobal->EventTargetFor(TaskCategory::Other)),
      mOriginalInputStream(aInputStream) {
  MOZ_DIAGNOSTIC_ASSERT(aInputStream);
  MOZ_DIAGNOSTIC_ASSERT(aStreamHolder);
}

void BodyStream::ErrorPropagation(JSContext* aCx,
                                  const MutexSingleWriterAutoLock& aProofOfLock,
                                  ReadableStream* aStream, nsresult aError) {
  mMutex.AssertOnWritingThread();
  mMutex.AssertCurrentThreadOwns();

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
  ErrorResult rv;
  // XXXbz can we come up with a better error message here to tell the
  // consumer what went wrong?
  rv.ThrowTypeError("Error in body stream");

  JS::Rooted<JS::Value> errorValue(aCx);
  bool ok = ToJSValue(aCx, std::move(rv), &errorValue);
  MOZ_RELEASE_ASSERT(ok, "ToJSValue never fails for ErrorResult");

  {
    MutexSingleWriterAutoUnlock unlock(mMutex);
    // Don't re-error an already errored stream.
    if (aStream->State() == ReadableStream::ReaderState::Readable) {
      IgnoredErrorResult rv;
      ReadableStreamError(aCx, aStream, errorValue, rv);
      NS_WARNING_ASSERTION(!rv.Failed(), "Failed to error BodyStream");
    }
  }

  ReleaseObjects(aProofOfLock);
}

void BodyStream::EnqueueChunkWithSizeIntoStream(JSContext* aCx,
                                                ReadableStream* aStream,
                                                uint64_t aAvailableData,
                                                ErrorResult& aRv) {
  // To avoid OOMing up on huge amounts of available data on a 32 bit system,
  // as well as potentially overflowing nsIInputStream's Read method's
  // parameter, let's limit our maximum chunk size to 256MB.
  uint32_t ableToRead =
      std::min(static_cast<uint64_t>(256 * 1024 * 1024), aAvailableData);

  // Create Chunk
  aRv.MightThrowJSException();
  JS::Rooted<JSObject*> chunk(aCx, JS_NewUint8Array(aCx, ableToRead));
  if (!chunk) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  {
    uint32_t bytesWritten = 0;

    WriteIntoReadRequestBuffer(aCx, aStream, chunk, ableToRead, &bytesWritten);

    // If bytesWritten is zero, then the stream has been closed; return
    // rather than enqueueing a chunk filled with zeros.
    if (bytesWritten == 0) {
      return;
    }

    // If we don't read every byte we've allocated in the Uint8Array
    // we risk enqueing a chunk that is padded with trailing zeros,
    // corrupting future processing of the chunks:
    MOZ_DIAGNOSTIC_ASSERT((ableToRead - bytesWritten) == 0);
  }

  MOZ_ASSERT(aStream->Controller()->IsByte());
  RefPtr<ReadableByteStreamController> byteStreamController =
      aStream->Controller()->AsByte();

  ReadableByteStreamControllerEnqueue(aCx, byteStreamController, chunk, aRv);
  if (aRv.Failed()) {
    return;
  }
}

// thread-safety doesn't handle emplace well
NS_IMETHODIMP
BodyStream::OnInputStreamReady(nsIAsyncInputStream* aStream)
    MOZ_NO_THREAD_SAFETY_ANALYSIS {
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(aStream);
  mAsyncWaitWorkerRef = nullptr;

  // Acquire |mMutex| in order to safely inspect |mState| and use |mGlobal|.
  Maybe<MutexSingleWriterAutoLock> lock;
  lock.emplace(mMutex);

  // Already closed. We have nothing else to do here.
  if (mState == eClosed) {
    return NS_OK;
  }

  // Perform a microtask checkpoint after all actions are completed.  Note that
  // |mMutex| *must not* be held when the checkpoint occurs -- hence, far down,
  // the |lock.reset()|.  (|MutexAutoUnlock| as RAII wouldn't work for this task
  // because its destructor would reacquire |mMutex| before these objects'
  // destructors run.)
  nsAutoMicroTask mt;
  AutoEntryScript aes(mGlobal, "fetch body data available");

  MOZ_DIAGNOSTIC_ASSERT(mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(mState == eReading || mState == eChecking);

  JSContext* cx = aes.cx();
  ReadableStream* stream = mStreamHolder->GetReadableStreamBody();
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  uint64_t size = 0;
  nsresult rv = mInputStream->Available(&size);
  if (NS_SUCCEEDED(rv) && size == 0) {
    // In theory this should not happen. If size is 0, the stream should be
    // considered closed.
    rv = NS_BASE_STREAM_CLOSED;
  }

  // No warning for stream closed.
  if (rv == NS_BASE_STREAM_CLOSED || NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(cx, *lock, stream, rv);
    return NS_OK;
  }

  // This extra checking is completed. Let's wait for the next read request.
  if (mState == eChecking) {
    mState = eWaiting;
    return NS_OK;
  }

  mState = eWriting;

  // Release the mutex before the call below (which could execute JS), as well
  // as before the microtask checkpoint queued up above occurs.
  lock.reset();
  ErrorResult errorResult;
  EnqueueChunkWithSizeIntoStream(cx, stream, size, errorResult);
  errorResult.WouldReportJSException();
  if (errorResult.Failed()) {
    lock.emplace(mMutex);
    ErrorPropagation(cx, *lock, stream, errorResult.StealNSResult());
    return NS_OK;
  }

  // The previous call can execute JS (even up to running a nested event
  // loop), so |mState| can't be asserted to have any particular value, even
  // if the previous call succeeds.

  return NS_OK;
}

/* static */
nsresult BodyStream::RetrieveInputStream(BodyStreamHolder* aStream,
                                         nsIInputStream** aInputStream) {
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aInputStream);
  BodyStream* stream = aStream->GetBodyStream();
  if (NS_WARN_IF(!stream)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

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

void BodyStream::Close() {
  AssertIsOnOwningThread();

  MutexSingleWriterAutoLock lock(mMutex);

  if (mState == eClosed) {
    return;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    ReleaseObjects(lock);
    return;
  }
  ReadableStream* stream = mStreamHolder->GetReadableStreamBody();
  if (stream) {
    JSContext* cx = jsapi.cx();
    CloseAndReleaseObjects(cx, lock, stream);
  } else {
    ReleaseObjects(lock);
  }
}

void BodyStream::CloseAndReleaseObjects(
    JSContext* aCx, const MutexSingleWriterAutoLock& aProofOfLock,
    ReadableStream* aStream) {
  AssertIsOnOwningThread();
  mMutex.AssertCurrentThreadOwns();
  MOZ_DIAGNOSTIC_ASSERT(mState != eClosed);

  ReleaseObjects(aProofOfLock);

  MutexSingleWriterAutoUnlock unlock(mMutex);

  if (aStream->State() == ReadableStream::ReaderState::Readable) {
    IgnoredErrorResult rv;
    ReadableStreamClose(aCx, aStream, rv);
    NS_WARNING_ASSERTION(!rv.Failed(), "Failed to Close Stream");
  }
}

void BodyStream::ReleaseObjects() {
  MutexSingleWriterAutoLock lock(mMutex);
  ReleaseObjects(lock);
}

void BodyStream::ReleaseObjects(const MutexSingleWriterAutoLock& aProofOfLock) {
  // This method can be called on 2 possible threads: the owning one and a JS
  // thread used to release resources. If we are on the JS thread, we need to
  // dispatch a runnable to go back to the owning thread in order to release
  // resources correctly.

  if (mState == eClosed) {
    // Already gone. Nothing to do.
    return;
  }

  if (!NS_IsMainThread() && !IsCurrentThreadRunningWorker()) {
    // Let's dispatch a WorkerControlRunnable if the owning thread is a worker.
    if (mWorkerRef) {
      RefPtr<WorkerShutdown> r =
          new WorkerShutdown(mWorkerRef->Private(), this);
      Unused << NS_WARN_IF(!r->Dispatch());
      return;
    }

    // A normal runnable of the owning thread is the main-thread.
    RefPtr<BodyStream> self = this;
    RefPtr<Runnable> r = NS_NewRunnableFunction(
        "BodyStream::ReleaseObjects", [self]() { self->ReleaseObjects(); });
    mOwningEventTarget->Dispatch(r.forget());
    return;
  }

  AssertIsOnOwningThread();

  mState = eClosed;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
    }
  }

  ReadableStream* stream = mStreamHolder->GetReadableStreamBody();
  if (stream) {
    stream->ReleaseObjects();
  }

  mWorkerRef = nullptr;
  mGlobal = nullptr;

  // Since calling ForgetBodyStream can cause our current ref count to drop to
  // zero, which would be bad, because this means we'd be destroying the mutex
  // which aProofOfLock is holding; instead, we do this later by creating an
  // event.
  GetCurrentSerialEventTarget()->Dispatch(NS_NewCancelableRunnableFunction(
      "BodyStream::ReleaseObjects",
      [streamHolder = RefPtr{mStreamHolder->TakeBodyStream()}] {
        // Intentionally left blank: The destruction of this lambda will free
        // free the stream holder, thus releasing the bodystream.
        //
        // This is cancelable because if a worker cancels this, we're still fine
        // as the lambda will be successfully destroyed.
      }));
  mStreamHolder->NullifyStream();
  mStreamHolder = nullptr;
}

#ifdef DEBUG
void BodyStream::AssertIsOnOwningThread() const {
  NS_ASSERT_OWNINGTHREAD(BodyStream);
}
#endif

// nsIObserver
// -----------

NS_IMETHODIMP
BodyStream::Observe(nsISupports* aSubject, const char* aTopic,
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

}  // namespace mozilla::dom
