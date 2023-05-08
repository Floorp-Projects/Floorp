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
#include <utility>

namespace mozilla::dom {

// BodyStreamHolder
// ---------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(BodyStreamHolder)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BodyStreamHolder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReadableStreamBody)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BodyStreamHolder)
  if (tmp->mBodyStream) {
    tmp->mBodyStream->ReleaseObjects();
    MOZ_ASSERT(!tmp->mBodyStream);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReadableStreamBody);
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
//
// This class has thread ownership assertions everywhere because historically it
// could be touched by random threads. See also:
// https://searchfox.org/mozilla-esr60/rev/02b4ae79b24aae2346b1338e2bf095a571192061/dom/fetch/FetchStream.cpp#302,306,312
//
// ---------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(BodyStream, nsIInputStreamCallback, nsIObserver,
                  nsISupportsWeakReference)

class BodyStreamUnderlyingSourceAlgorithms final
    : public UnderlyingSourceAlgorithmsWrapper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BodyStreamUnderlyingSourceAlgorithms,
                                           UnderlyingSourceAlgorithmsBase)

  BodyStreamUnderlyingSourceAlgorithms(nsIGlobalObject& aGlobal,
                                       BodyStreamHolder& aUnderlyingSource)
      : mGlobal(&aGlobal), mUnderlyingSource(&aUnderlyingSource) {}

  already_AddRefed<Promise> PullCallbackImpl(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override {
    RefPtr<BodyStream> bodyStream = mUnderlyingSource->GetBodyStream();
    return bodyStream->PullCallback(aCx, aController, aRv);
  }

  void ReleaseObjects() override {
    RefPtr<BodyStreamHolder> holder = mUnderlyingSource.forget();
    // BodyStream may not be available if this cleanup happened first from
    // BodyStream side.
    if (RefPtr<BodyStream> bodyStream = holder->GetBodyStream()) {
      bodyStream->CloseInputAndReleaseObjects();
    }
  }

  BodyStreamHolder* GetBodyStreamHolder() override { return mUnderlyingSource; }

 protected:
  ~BodyStreamUnderlyingSourceAlgorithms() override = default;

 private:
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<BodyStreamHolder> mUnderlyingSource;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(BodyStreamUnderlyingSourceAlgorithms,
                                   UnderlyingSourceAlgorithmsBase, mGlobal,
                                   mUnderlyingSource)
NS_IMPL_ADDREF_INHERITED(BodyStreamUnderlyingSourceAlgorithms,
                         UnderlyingSourceAlgorithmsBase)
NS_IMPL_RELEASE_INHERITED(BodyStreamUnderlyingSourceAlgorithms,
                          UnderlyingSourceAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BodyStreamUnderlyingSourceAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourceAlgorithmsBase)

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

    workerPrivate->AssertIsOnWorkerThread();

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

  auto algorithms = MakeRefPtr<BodyStreamUnderlyingSourceAlgorithms>(
      *aGlobal, *aStreamHolder);
  RefPtr<ReadableStream> body = ReadableStream::CreateByteNative(
      aCx, aGlobal, *algorithms, Nothing(), aRv);
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
  ReadableStream* stream = aController.Stream();
  MOZ_ASSERT(stream);

  MOZ_DIAGNOSTIC_ASSERT(stream->Disturbed());

  MOZ_DIAGNOSTIC_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  MOZ_DIAGNOSTIC_ASSERT(!IsClosed());
  MOZ_ASSERT(!mPullPromise);
  mPullPromise = Promise::CreateInfallible(aController.GetParentObject());

  // Reading the stream, let's mark it as such
  mStreamHolder->MarkAsRead();

  if (!mInputStream) {
    // This is the first use of the stream. Let's convert the
    // mOriginalInputStream into an nsIAsyncInputStream.
    MOZ_ASSERT(mOriginalInputStream);

    nsCOMPtr<nsIAsyncInputStream> asyncStream;
    nsresult rv = NS_MakeAsyncNonBlockingInputStream(
        mOriginalInputStream.forget(), getter_AddRefs(asyncStream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ErrorPropagation(aCx, stream, rv);
      return nullptr;
    }

    mInputStream = asyncStream;
    mOriginalInputStream = nullptr;
  }

  MOZ_DIAGNOSTIC_ASSERT(mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(!mOriginalInputStream);

  nsresult rv = mInputStream->AsyncWait(this, 0, 0, mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(aCx, stream, rv);
    return nullptr;
  }
  mAsyncWaitWorkerRef = mWorkerRef;

  // All good.
  return do_AddRef(mPullPromise);
}

void BodyStream::WriteIntoReadRequestBuffer(JSContext* aCx,
                                            ReadableStream* aStream,
                                            JS::Handle<JSObject*> aBuffer,
                                            uint32_t aLength,
                                            uint32_t* aByteWritten) {
  MOZ_DIAGNOSTIC_ASSERT(aBuffer);
  MOZ_DIAGNOSTIC_ASSERT(aByteWritten);

  MOZ_DIAGNOSTIC_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  MOZ_DIAGNOSTIC_ASSERT(mInputStream);
  MOZ_DIAGNOSTIC_ASSERT(!IsClosed());
  MOZ_DIAGNOSTIC_ASSERT(mPullPromise->State() ==
                        Promise::PromiseState::Pending);

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

    buffer = JS_GetArrayBufferViewData(aBuffer, &isSharedMemory, noGC);
    MOZ_ASSERT(!isSharedMemory);

    rv = mInputStream->Read(static_cast<char*>(buffer), aLength, &written);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ErrorPropagation(aCx, aStream, rv);
      return;
    }
  }

  *aByteWritten = written;

  if (written == 0) {
    CloseAndReleaseObjects(aCx, aStream);
    return;
  }

  // All good.
}

void BodyStream::CloseInputAndReleaseObjects() {
  MOZ_DIAGNOSTIC_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  if (mStreamHolder) {
    // Being closed means someone touched the stream, let's mark it as read.
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
}

BodyStream::BodyStream(nsIGlobalObject* aGlobal,
                       BodyStreamHolder* aStreamHolder,
                       nsIInputStream* aInputStream)
    : mGlobal(aGlobal),
      mStreamHolder(aStreamHolder),
      mOwningEventTarget(aGlobal->EventTargetFor(TaskCategory::Other)),
      mOriginalInputStream(aInputStream) {
  MOZ_DIAGNOSTIC_ASSERT(aInputStream);
  MOZ_DIAGNOSTIC_ASSERT(aStreamHolder);
}

BodyStream::~BodyStream() {
  MOZ_DIAGNOSTIC_ASSERT(mOwningEventTarget->IsOnCurrentThread());
}

void BodyStream::ErrorPropagation(JSContext* aCx, ReadableStream* aStream,
                                  nsresult aError) {
  MOZ_DIAGNOSTIC_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  // Nothing to do.
  if (IsClosed()) {
    return;
  }

  // Let's close the stream.
  if (aError == NS_BASE_STREAM_CLOSED) {
    CloseAndReleaseObjects(aCx, aStream);
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
    // This will be ignored if it's already errored.
    IgnoredErrorResult rv;
    aStream->ErrorNative(aCx, errorValue, rv);
    NS_WARNING_ASSERTION(!rv.Failed(), "Failed to error BodyStream");
  }

  if (mStreamHolder) {
    // Being errored means someone touched the stream, let's mark it as read.
    mStreamHolder->MarkAsRead();
  }

  if (mInputStream) {
    mInputStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
  }

  ReleaseObjects();
}

// https://fetch.spec.whatwg.org/#concept-bodyinit-extract
// Step 12.1: Whenever one or more bytes are available and stream is not
// errored, enqueue a Uint8Array wrapping an ArrayBuffer containing the
// available bytes into stream.
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
    // we risk enqueuing a chunk that is padded with trailing zeros,
    // corrupting future processing of the chunks:
    MOZ_DIAGNOSTIC_ASSERT((ableToRead - bytesWritten) == 0);
  }

  MOZ_ASSERT(aStream->Controller()->IsByte());
  JS::Rooted<JS::Value> chunkValue(aCx);
  chunkValue.setObject(*chunk);
  aStream->EnqueueNative(aCx, chunkValue, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Subscribe WAIT_CLOSURE_ONLY so that OnInputStreamReady can be called when
  // mInputStream is closed.
  nsresult rv = mInputStream->AsyncWait(
      this, nsIAsyncInputStream::WAIT_CLOSURE_ONLY, 0, mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  mAsyncWaitWorkerRef = mWorkerRef;
}

NS_IMETHODIMP
BodyStream::OnInputStreamReady(nsIAsyncInputStream* aStream) {
  MOZ_DIAGNOSTIC_ASSERT(mOwningEventTarget->IsOnCurrentThread());
  MOZ_DIAGNOSTIC_ASSERT(aStream);
  mAsyncWaitWorkerRef = nullptr;

  // Already closed. We have nothing else to do here.
  if (IsClosed()) {
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

  JSContext* cx = aes.cx();
  ReadableStream* stream = mStreamHolder->GetReadableStreamBody();
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  uint64_t size = 0;
  nsresult rv = mInputStream->Available(&size);
  MOZ_ASSERT_IF(NS_SUCCEEDED(rv), size > 0);

  // No warning for stream closed.
  if (rv == NS_BASE_STREAM_CLOSED || NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(cx, stream, rv);
    return NS_OK;
  }

  // Not having a promise means we are pinged by stream closure
  // (WAIT_CLOSURE_ONLY below), but here we still have more data to read. Let's
  // wait for the next read request in that case.
  if (!mPullPromise) {
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(mPullPromise->State() ==
                        Promise::PromiseState::Pending);

  ErrorResult errorResult;
  EnqueueChunkWithSizeIntoStream(cx, stream, size, errorResult);
  errorResult.WouldReportJSException();
  if (errorResult.Failed()) {
    ErrorPropagation(cx, stream, errorResult.StealNSResult());
    return NS_OK;
  }

  // Enqueuing triggers read request chunk steps which may execute JS, but:
  // 1. The nsIAsyncInputStream should hold the reference of `this` so it should
  // be safe from cycle collection
  // 2. AsyncWait is called after enqueuing and thus OnInputStreamReady can't be
  // synchronously called again
  //
  // That said, it's generally good to be cautious as there's no guarantee that
  // the interface is implemented in a safest way.
  MOZ_DIAGNOSTIC_ASSERT(mPullPromise);
  if (mPullPromise) {
    mPullPromise->MaybeResolveWithUndefined();
    mPullPromise = nullptr;
  }

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

  MOZ_DIAGNOSTIC_ASSERT(stream->mOwningEventTarget->IsOnCurrentThread());

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
  MOZ_DIAGNOSTIC_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  if (IsClosed()) {
    return;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    ReleaseObjects();
    return;
  }
  ReadableStream* stream = mStreamHolder->GetReadableStreamBody();
  if (stream) {
    JSContext* cx = jsapi.cx();
    CloseAndReleaseObjects(cx, stream);
  } else {
    ReleaseObjects();
  }
}

void BodyStream::CloseAndReleaseObjects(JSContext* aCx,
                                        ReadableStream* aStream) {
  MOZ_DIAGNOSTIC_ASSERT(mOwningEventTarget->IsOnCurrentThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsClosed());

  ReleaseObjects();

  if (aStream->State() == ReadableStream::ReaderState::Readable) {
    IgnoredErrorResult rv;
    aStream->CloseNative(aCx, rv);
    NS_WARNING_ASSERTION(!rv.Failed(), "Failed to Close Stream");
  }
}

void BodyStream::ReleaseObjects() {
  MOZ_DIAGNOSTIC_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  if (IsClosed()) {
    // Already gone. Nothing to do.
    return;
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
    }
  }

  mWorkerRef = nullptr;
  mGlobal = nullptr;
  // It's okay to leave a potentially unsettled promise as-is as this is only
  // used to prevent reentrant to PullCallback. CloseNative() or ErrorNative()
  // will settle the read requests for us.
  mPullPromise = nullptr;

  RefPtr<BodyStream> self = mStreamHolder->TakeBodyStream();
  mStreamHolder->NullifyStream();
  mStreamHolder = nullptr;
}

// nsIObserver
// -----------

NS_IMETHODIMP
BodyStream::Observe(nsISupports* aSubject, const char* aTopic,
                    const char16_t* aData) {
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  MOZ_ASSERT(strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (SameCOMIdentity(aSubject, window)) {
    Close();
  }

  return NS_OK;
}

}  // namespace mozilla::dom
