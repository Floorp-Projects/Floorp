/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamUtils.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "js/experimental/TypedData.h"

namespace mozilla::dom {

// UnderlyingSourceAlgorithmsBase
NS_IMPL_CYCLE_COLLECTION(UnderlyingSourceAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSourceAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSourceAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSourceAlgorithmsBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_INHERITED_WITH_JS_MEMBERS(
    UnderlyingSourceAlgorithms, UnderlyingSourceAlgorithmsBase,
    (mGlobal, mStartCallback, mPullCallback, mCancelCallback),
    (mUnderlyingSource))
NS_IMPL_ADDREF_INHERITED(UnderlyingSourceAlgorithms,
                         UnderlyingSourceAlgorithmsBase)
NS_IMPL_RELEASE_INHERITED(UnderlyingSourceAlgorithms,
                          UnderlyingSourceAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSourceAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourceAlgorithmsBase)

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
void UnderlyingSourceAlgorithms::StartCallback(
    JSContext* aCx, ReadableStreamController& aController,
    JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv) {
  if (!mStartCallback) {
    // Step 2: Let startAlgorithm be an algorithm that returns undefined.
    aRetVal.setUndefined();
    return;
  }

  // Step 5: If underlyingSourceDict["start"] exists, then set startAlgorithm to
  // an algorithm which returns the result of invoking
  // underlyingSourceDict["start"] with argument list « controller » and
  // callback this value underlyingSource.
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSource);
  ReadableStreamDefaultControllerOrReadableByteStreamController controller;
  if (aController.IsDefault()) {
    controller.SetAsReadableStreamDefaultController() = aController.AsDefault();
  } else {
    controller.SetAsReadableByteStreamController() = aController.AsByte();
  }

  return mStartCallback->Call(thisObj, controller, aRetVal, aRv,
                              "UnderlyingSource.start",
                              CallbackFunction::eRethrowExceptions);
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
already_AddRefed<Promise> UnderlyingSourceAlgorithms::PullCallback(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSource);
  if (!mPullCallback) {
    // Step 3: Let pullAlgorithm be an algorithm that returns a promise resolved
    // with undefined.
    return Promise::CreateResolvedWithUndefined(mGlobal, aRv);
  }

  // Step 6: If underlyingSourceDict["pull"] exists, then set pullAlgorithm to
  // an algorithm which returns the result of invoking
  // underlyingSourceDict["pull"] with argument list « controller » and callback
  // this value underlyingSource.
  ReadableStreamDefaultControllerOrReadableByteStreamController controller;
  if (aController.IsDefault()) {
    controller.SetAsReadableStreamDefaultController() = aController.AsDefault();
  } else {
    controller.SetAsReadableByteStreamController() = aController.AsByte();
  }

  RefPtr<Promise> promise =
      mPullCallback->Call(thisObj, controller, aRv, "UnderlyingSource.pull",
                          CallbackFunction::eRethrowExceptions);

  return promise.forget();
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
already_AddRefed<Promise> UnderlyingSourceAlgorithms::CancelCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  if (!mCancelCallback) {
    // Step 4: Let cancelAlgorithm be an algorithm that returns a promise
    // resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mGlobal, aRv);
  }

  // Step 7: If underlyingSourceDict["cancel"] exists, then set cancelAlgorithm
  // to an algorithm which takes an argument reason and returns the result of
  // invoking underlyingSourceDict["cancel"] with argument list « reason » and
  // callback this value underlyingSource.
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSource);
  RefPtr<Promise> promise =
      mCancelCallback->Call(thisObj, aReason, aRv, "UnderlyingSource.cancel",
                            CallbackFunction::eRethrowExceptions);

  return promise.forget();
}

// Shared between:
// https://streams.spec.whatwg.org/#readablestream-set-up
// https://streams.spec.whatwg.org/#readablestream-set-up-with-byte-reading-support
// Step 1: Let startAlgorithm be an algorithm that returns undefined.
void UnderlyingSourceAlgorithmsWrapper::StartCallback(
    JSContext*, ReadableStreamController&, JS::MutableHandle<JS::Value> aRetVal,
    ErrorResult&) {
  aRetVal.setUndefined();
}

// Shared between:
// https://streams.spec.whatwg.org/#readablestream-set-up
// https://streams.spec.whatwg.org/#readablestream-set-up-with-byte-reading-support
// Step 2: Let pullAlgorithmWrapper be an algorithm that runs these steps:
already_AddRefed<Promise> UnderlyingSourceAlgorithmsWrapper::PullCallback(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = aController.GetParentObject();
  return PromisifyAlgorithm(
      global,
      [&](ErrorResult& aRv) MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
        return PullCallbackImpl(aCx, aController, aRv);
      },
      aRv);
}

// Shared between:
// https://streams.spec.whatwg.org/#readablestream-set-up
// https://streams.spec.whatwg.org/#readablestream-set-up-with-byte-reading-support
// Step 3: Let cancelAlgorithmWrapper be an algorithm that runs these steps:
already_AddRefed<Promise> UnderlyingSourceAlgorithmsWrapper::CancelCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(aCx);
  return PromisifyAlgorithm(
      global,
      [&](ErrorResult& aRv) { return CancelCallbackImpl(aCx, aReason, aRv); },
      aRv);
}

NS_IMPL_ISUPPORTS(InputStreamHolder, nsIInputStreamCallback)

InputStreamHolder::InputStreamHolder(JSContext* aCx,
                                     InputToReadableStreamAlgorithms* aCallback,
                                     nsIAsyncInputStream* aInput)
    : mCallback(aCallback), mInput(aInput) {
  if (!NS_IsMainThread()) {
    // We're in a worker
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    MOZ_ASSERT(workerPrivate);

    workerPrivate->AssertIsOnWorkerThread();

    // Note, this will create a ref-cycle between the holder and the stream.
    // The cycle is broken when the stream is closed or the worker begins
    // shutting down.
    mWorkerRef =
        StrongWorkerRef::Create(workerPrivate, "InputStreamHolder",
                                [self = RefPtr{this}]() { self->Shutdown(); });
    if (NS_WARN_IF(!mWorkerRef)) {
      return;
    }
  }
}

InputStreamHolder::~InputStreamHolder() { MOZ_ASSERT(!mCallback); }

void InputStreamHolder::Shutdown() {
  if (mCallback) {
    mCallback = nullptr;
    mInput->CloseWithStatus(NS_BASE_STREAM_CLOSED);
    mWorkerRef = nullptr;
    // If we have an AsyncWait running, we'll get a callback and clear
    // the mAsyncWaitWorkerRef
  }
}

nsresult InputStreamHolder::AsyncWait(uint32_t aFlags, uint32_t aRequestedCount,
                                      nsIEventTarget* aEventTarget) {
  nsresult rv = mInput->AsyncWait(this, aFlags, aRequestedCount, aEventTarget);
  if (NS_SUCCEEDED(rv)) {
    mAsyncWaitWorkerRef = mWorkerRef;
  }
  return rv;
}

NS_IMETHODIMP InputStreamHolder::OnInputStreamReady(
    nsIAsyncInputStream* aStream) {
  mAsyncWaitWorkerRef = nullptr;
  // We may get called back after ::Shutdown()
  if (mCallback) {
    return mCallback->OnInputStreamReady(aStream);
  }
  return NS_ERROR_FAILURE;
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(
    InputToReadableStreamAlgorithms, UnderlyingSourceAlgorithmsWrapper)
NS_IMPL_CYCLE_COLLECTION_INHERITED(InputToReadableStreamAlgorithms,
                                   UnderlyingSourceAlgorithmsWrapper, mStream,
                                   mOwningEventTarget)

already_AddRefed<Promise> InputToReadableStreamAlgorithms::PullCallbackImpl(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  MOZ_ASSERT(aController.IsByte());
  ReadableStream* stream = aController.Stream();
  MOZ_ASSERT(stream);

  MOZ_DIAGNOSTIC_ASSERT(stream->Disturbed());

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
    MOZ_ASSERT(mInput);
    mState = eReading;

    return resolvedWithUndefinedPromise.forget();
  }

  mState = eReading;

  MOZ_DIAGNOSTIC_ASSERT(mInput);

  nsresult rv = mInput->AsyncWait(0, 0, mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(aCx, stream, rv);
    return nullptr;
  }

  // All good.
  return resolvedWithUndefinedPromise.forget();
}

NS_IMETHODIMP
InputToReadableStreamAlgorithms::OnInputStreamReady(
    nsIAsyncInputStream* aStream) {
  MOZ_DIAGNOSTIC_ASSERT(aStream);

  // Already closed. We have nothing else to do here.
  if (mState == eClosed || !mStream) {
    return NS_OK;
  }

  AutoEntryScript aes(mStream->GetParentObject(),
                      "InputToReadableStream data available");

  MOZ_DIAGNOSTIC_ASSERT(mInput);
  MOZ_DIAGNOSTIC_ASSERT(mState == eReading || mState == eChecking);

  JSContext* cx = aes.cx();

  uint64_t size = 0;
  nsresult rv = mInput->Available(&size);
  if (NS_SUCCEEDED(rv) && size == 0) {
    // In theory this should not happen. If size is 0, the stream should be
    // considered closed.
    rv = NS_BASE_STREAM_CLOSED;
  }

  // No warning for stream closed.
  if (rv == NS_BASE_STREAM_CLOSED || NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(cx, mStream, rv);
    return NS_OK;
  }

  // This extra checking is completed. Let's wait for the next read request.
  if (mState == eChecking) {
    mState = eWaiting;
    return NS_OK;
  }

  mState = eWriting;

  ErrorResult errorResult;
  EnqueueChunkWithSizeIntoStream(cx, mStream, size, errorResult);
  errorResult.WouldReportJSException();
  if (errorResult.Failed()) {
    ErrorPropagation(cx, mStream, errorResult.StealNSResult());
    return NS_OK;
  }

  // The previous call can execute JS (even up to running a nested event
  // loop), so |mState| can't be asserted to have any particular value, even
  // if the previous call succeeds.

  return NS_OK;
}

void InputToReadableStreamAlgorithms::WriteIntoReadRequestBuffer(
    JSContext* aCx, ReadableStream* aStream, JS::Handle<JSObject*> aBuffer,
    uint32_t aLength, uint32_t* aByteWritten) {
  MOZ_DIAGNOSTIC_ASSERT(aBuffer);
  MOZ_DIAGNOSTIC_ASSERT(aByteWritten);
  MOZ_DIAGNOSTIC_ASSERT(mInput);
  MOZ_DIAGNOSTIC_ASSERT(mState == eWriting);
  mState = eChecking;

  uint32_t written;
  nsresult rv;
  void* buffer;
  {
    // Bug 1754513: Hazard suppression.
    //
    // Because mInput->Read is detected as possibly GCing by the
    // current state of our static hazard analysis, we need to do the
    // suppression here. This can be removed with future improvements
    // to the static analysis.
    JS::AutoSuppressGCAnalysis suppress;
    JS::AutoCheckCannotGC noGC;
    bool isSharedMemory;

    buffer = JS_GetArrayBufferViewData(aBuffer, &isSharedMemory, noGC);
    MOZ_ASSERT(!isSharedMemory);

    rv = mInput->Read(static_cast<char*>(buffer), aLength, &written);
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

  rv = mInput->AsyncWait(0, 0, mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(aCx, aStream, rv);
    return;
  }

  // All good.
}

// Whenever one or more bytes are available and stream is not
// errored, enqueue a Uint8Array wrapping an ArrayBuffer containing the
// available bytes into stream.
void InputToReadableStreamAlgorithms::EnqueueChunkWithSizeIntoStream(
    JSContext* aCx, ReadableStream* aStream, uint64_t aAvailableData,
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
}

void InputToReadableStreamAlgorithms::CloseAndReleaseObjects(
    JSContext* aCx, ReadableStream* aStream) {
  MOZ_DIAGNOSTIC_ASSERT(mState != eClosed);

  mState = eClosed;
  ReleaseObjects();

  if (aStream->State() == ReadableStream::ReaderState::Readable) {
    IgnoredErrorResult rv;
    aStream->CloseNative(aCx, rv);
    NS_WARNING_ASSERTION(!rv.Failed(), "Failed to Close Stream");
  }
}

void InputToReadableStreamAlgorithms::ReleaseObjects() {
  if (mInput) {
    mInput->CloseWithStatus(NS_BASE_STREAM_CLOSED);
    mInput->Shutdown();
    mInput = nullptr;
  }
}

void InputToReadableStreamAlgorithms::ErrorPropagation(JSContext* aCx,
                                                       ReadableStream* aStream,
                                                       nsresult aError) {
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
  ErrorResult rv;
  // XXXbz can we come up with a better error message here to tell the
  // consumer what went wrong?
  rv.ThrowTypeError("Error in input stream");

  JS::Rooted<JS::Value> errorValue(aCx);
  bool ok = ToJSValue(aCx, std::move(rv), &errorValue);
  MOZ_RELEASE_ASSERT(ok, "ToJSValue never fails for ErrorResult");

  {
    // This will be ignored if it's already errored.
    IgnoredErrorResult rv;
    aStream->ErrorNative(aCx, errorValue, rv);
    NS_WARNING_ASSERTION(!rv.Failed(), "Failed to error InputToReadableStream");
  }

  MOZ_ASSERT(mInput);
  CloseAndReleaseObjects(aCx, aStream);
}

}  // namespace mozilla::dom
