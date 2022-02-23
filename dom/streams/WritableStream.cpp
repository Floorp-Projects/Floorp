/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WritableStream.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/AbortSignal.h"
#include "mozilla/dom/BindingCallContext.h"
#include "mozilla/dom/ModuleMapKey.h"
#include "mozilla/dom/QueueWithSizes.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "mozilla/dom/ReadRequest.h"
#include "mozilla/dom/StreamUtils.h"
#include "mozilla/dom/UnderlyingSinkBinding.h"
#include "mozilla/dom/WritableStreamBinding.h"
#include "mozilla/dom/WritableStreamDefaultController.h"
#include "mozilla/dom/WritableStreamDefaultWriter.h"
#include "nsCOMPtr.h"

#include "mozilla/dom/Promise-inl.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(WritableStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WritableStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal, mCloseRequest, mController,
                                  mInFlightWriteRequest, mInFlightCloseRequest,
                                  mPendingAbortRequestPromise, mWriter,
                                  mWriteRequests)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mPendingAbortRequestReason.setNull();
  tmp->mStoredError.setNull();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WritableStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(
      mGlobal, mCloseRequest, mController, mInFlightWriteRequest,
      mInFlightCloseRequest, mWriter, mWriteRequests)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WritableStream)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPendingAbortRequestReason)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mStoredError)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WritableStream)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WritableStream)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WritableStream)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

WritableStream::WritableStream(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
  mozilla::HoldJSObjects(this);
}

WritableStream::WritableStream(const GlobalObject& aGlobal)
    : mGlobal(do_QueryInterface(aGlobal.GetAsSupports())) {
  mozilla::HoldJSObjects(this);
}

WritableStream::~WritableStream() { mozilla::DropJSObjects(this); }

JSObject* WritableStream::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return WritableStream_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#writable-stream-deal-with-rejection
void WritableStream::DealWithRejection(JSContext* aCx,
                                       JS::Handle<JS::Value> aError,
                                       ErrorResult& aRv) {
  // Step 1. Let state be stream.[[state]].
  // Step 2. If state is "writable",
  if (mState == WriterState::Writable) {
    // Step 2.1. Perform ! WritableStreamStartErroring(stream, error).
    StartErroring(aCx, aError, aRv);

    // Step 2.2. Return.
    return;
  }

  // Step 3. Assert: state is "erroring".
  MOZ_ASSERT(mState == WriterState::Erroring);

  // Step 4. Perform ! WritableStreamFinishErroring(stream).
  FinishErroring(aCx, aRv);
}

class AbortStepsNativePromiseHandler final : public PromiseNativeHandler {
  ~AbortStepsNativePromiseHandler() = default;

  RefPtr<WritableStream> mStream;
  RefPtr<Promise> mAbortRequestPromise;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AbortStepsNativePromiseHandler)

  explicit AbortStepsNativePromiseHandler(WritableStream* aStream,
                                          Promise* aAbortRequestPromise)
      : PromiseNativeHandler(),
        mStream(aStream),
        mAbortRequestPromise(aAbortRequestPromise) {}

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#writable-stream-finish-erroring

    // Step 13. Upon fulfillment of promise,
    // Step 13.1. Resolve abortRequest’s promise with undefined.
    mAbortRequestPromise->MaybeResolveWithUndefined();

    // Step 13.2. Perform !
    // WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream).
    mStream->RejectCloseAndClosedPromiseIfNeeded();
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#writable-stream-finish-erroring

    // Step 14. Upon rejection of promise with reason reason,
    // Step 14.1. Reject abortRequest’s promise with reason.
    mAbortRequestPromise->MaybeReject(aValue);

    // Step 14.2. Perform !
    // WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream).
    mStream->RejectCloseAndClosedPromiseIfNeeded();
  }
};

NS_IMPL_CYCLE_COLLECTION(AbortStepsNativePromiseHandler, mStream,
                         mAbortRequestPromise)
NS_IMPL_CYCLE_COLLECTING_ADDREF(AbortStepsNativePromiseHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AbortStepsNativePromiseHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortStepsNativePromiseHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#writable-stream-finish-erroring
void WritableStream::FinishErroring(JSContext* aCx, ErrorResult& aRv) {
  // Step 1. Assert: stream.[[state]] is "erroring".
  MOZ_ASSERT(mState == WriterState::Erroring);

  // Step 2. Assert: ! WritableStreamHasOperationMarkedInFlight(stream) is
  // false.
  MOZ_ASSERT(!HasOperationMarkedInFlight());

  // Step 3. Set stream.[[state]] to "errored".
  mState = WriterState::Errored;

  // Step 4. Perform ! stream.[[controller]].[[ErrorSteps]]().
  Controller()->ErrorSteps();

  // Step 5. Let storedError be stream.[[storedError]].
  JS::Rooted<JS::Value> storedError(aCx, mStoredError);

  // Step 6. For each writeRequest of stream.[[writeRequests]]:
  for (const RefPtr<Promise>& writeRequest : mWriteRequests) {
    // Step 6.1. Reject writeRequest with storedError.
    writeRequest->MaybeReject(storedError);
  }

  // Step 7. Set stream.[[writeRequests]] to an empty list.
  mWriteRequests.Clear();

  // Step 8. If stream.[[pendingAbortRequest]] is undefined,
  if (!mPendingAbortRequestPromise) {
    // Step 8.1. Perform !
    // WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream).
    RejectCloseAndClosedPromiseIfNeeded();

    // Step 8.2. Return.
    return;
  }

  // Step 9. Let abortRequest be stream.[[pendingAbortRequest]].
  RefPtr<Promise> abortPromise = mPendingAbortRequestPromise;
  JS::Rooted<JS::Value> abortReason(aCx, mPendingAbortRequestReason);
  bool abortWasAlreadyErroring = mPendingAbortRequestWasAlreadyErroring;

  // Step 10. Set stream.[[pendingAbortRequest]] to undefined.
  SetPendingAbortRequest(nullptr, JS::UndefinedHandleValue, false);

  // Step 11. If abortRequest’s was already erroring is true,
  if (abortWasAlreadyErroring) {
    // Step 11.1. Reject abortRequest’s promise with storedError.
    abortPromise->MaybeReject(storedError);

    // Step 11.2. Perform !
    // WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream).
    RejectCloseAndClosedPromiseIfNeeded();

    // Step 11.3. Return.
    return;
  }

  // Step 12. Let promise be !
  // stream.[[controller]].[[AbortSteps]](abortRequest’s reason).
  RefPtr<WritableStreamDefaultController> controller = mController;
  RefPtr<Promise> promise = controller->AbortSteps(aCx, abortReason, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 13 + 14.
  promise->AppendNativeHandler(
      new AbortStepsNativePromiseHandler(this, abortPromise));
}

// https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-close
void WritableStream::FinishInFlightClose() {
  // Step 1. Assert: stream.[[inFlightCloseRequest]] is not undefined.
  MOZ_ASSERT(mInFlightCloseRequest);

  // Step 2. Resolve stream.[[inFlightCloseRequest]] with undefined.
  mInFlightCloseRequest->MaybeResolveWithUndefined();

  // Step 3. Set stream.[[inFlightCloseRequest]] to undefined.
  mInFlightCloseRequest = nullptr;

  // Step 4. Let state be stream.[[state]].
  // Step 5. Assert: stream.[[state]] is "writable" or "erroring".
  MOZ_ASSERT(mState == WriterState::Writable ||
             mState == WriterState::Erroring);

  // Step 6. If state is "erroring",
  if (mState == WriterState::Erroring) {
    // Step 6.1. Set stream.[[storedError]] to undefined.
    mStoredError.setUndefined();

    // Step 6.2. If stream.[[pendingAbortRequest]] is not undefined,
    if (mPendingAbortRequestPromise) {
      // Step 6.2.1. Resolve stream.[[pendingAbortRequest]]'s promise with
      // undefined.
      mPendingAbortRequestPromise->MaybeResolveWithUndefined();

      // Step 6.2.2. Set stream.[[pendingAbortRequest]] to undefined.
      SetPendingAbortRequest(nullptr, JS::UndefinedHandleValue, false);
    }
  }

  // Step 7. Set stream.[[state]] to "closed".
  mState = WriterState::Closed;

  // Step 8. Let writer be stream.[[writer]].
  // Step 9. If writer is not undefined, resolve writer.[[closedPromise]] with
  // undefined.
  if (mWriter) {
    mWriter->ClosedPromise()->MaybeResolveWithUndefined();
  }

  // Step 10. Assert: stream.[[pendingAbortRequest]] is undefined.
  MOZ_ASSERT(!mPendingAbortRequestPromise);
  // Assert: stream.[[storedError]] is undefined.
  MOZ_ASSERT(mStoredError.isUndefined());
}

// https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-close-with-error
void WritableStream::FinishInFlightCloseWithError(JSContext* aCx,
                                                  JS::Handle<JS::Value> aError,
                                                  ErrorResult& aRv) {
  // Step 1. Assert: stream.[[inFlightCloseRequest]] is not undefined.
  MOZ_ASSERT(mInFlightCloseRequest);

  // Step 2. Reject stream.[[inFlightCloseRequest]] with error.
  mInFlightCloseRequest->MaybeReject(aError);

  // Step 3. Set stream.[[inFlightCloseRequest]] to undefined.
  mInFlightCloseRequest = nullptr;

  // Step 4. Assert: stream.[[state]] is "writable" or "erroring".
  MOZ_ASSERT(mState == WriterState::Writable ||
             mState == WriterState::Erroring);

  // Step 5. If stream.[[pendingAbortRequest]] is not undefined,
  if (mPendingAbortRequestPromise) {
    // Step 5.1. Reject stream.[[pendingAbortRequest]]'s promise with error.
    mPendingAbortRequestPromise->MaybeReject(aError);

    // Step 5.2. Set stream.[[pendingAbortRequest]] to undefined.
    SetPendingAbortRequest(nullptr, JS::UndefinedHandleValue, false);
  }

  // Step 6. Perform ! WritableStreamDealWithRejection(stream, error).
  DealWithRejection(aCx, aError, aRv);
}

// https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-write
void WritableStream::FinishInFlightWrite() {
  // Step 1. Assert: stream.[[inFlightWriteRequest]] is not undefined.
  MOZ_ASSERT(mInFlightWriteRequest);

  // Step 2. Resolve stream.[[inFlightWriteRequest]] with undefined.
  mInFlightWriteRequest->MaybeResolveWithUndefined();

  // Step 3. Set stream.[[inFlightWriteRequest]] to undefined.
  mInFlightWriteRequest = nullptr;
}

// https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-write-with-error
void WritableStream::FinishInFlightWriteWithError(JSContext* aCx,
                                                  JS::Handle<JS::Value> aError,
                                                  ErrorResult& aRv) {
  // Step 1. Assert: stream.[[inFlightWriteRequest]] is not undefined.
  MOZ_ASSERT(mInFlightWriteRequest);

  // Step 2. Reject stream.[[inFlightWriteRequest]] with error.
  mInFlightWriteRequest->MaybeReject(aError);

  // Step 3. Set stream.[[inFlightWriteRequest]] to undefined.
  mInFlightWriteRequest = nullptr;

  // Step 4. Assert: stream.[[state]] is "writable" or "erroring".
  MOZ_ASSERT(mState == WriterState::Writable ||
             mState == WriterState::Erroring);

  // Step 5. Perform ! WritableStreamDealWithRejection(stream, error).
  DealWithRejection(aCx, aError, aRv);
}

// https://streams.spec.whatwg.org/#writable-stream-mark-close-request-in-flight
void WritableStream::MarkCloseRequestInFlight() {
  // Step 1. Assert: stream.[[inFlightCloseRequest]] is undefined.
  MOZ_ASSERT(!mInFlightCloseRequest);

  // Step 2. Assert: stream.[[closeRequest]] is not undefined.
  MOZ_ASSERT(mCloseRequest);

  // Step 3. Set stream.[[inFlightCloseRequest]] to stream.[[closeRequest]].
  mInFlightCloseRequest = mCloseRequest;

  // Step 4. Set stream.[[closeRequest]] to undefined.
  mCloseRequest = nullptr;
}

// https://streams.spec.whatwg.org/#writable-stream-mark-first-write-request-in-flight
void WritableStream::MarkFirstWriteRequestInFlight() {
  // Step 1. Assert: stream.[[inFlightWriteRequest]] is undefined.
  MOZ_ASSERT(!mInFlightWriteRequest);

  // Step 2. Assert: stream.[[writeRequests]] is not empty.
  MOZ_ASSERT(!mWriteRequests.IsEmpty());

  // Step 3. Let writeRequest be stream.[[writeRequests]][0].
  RefPtr<Promise> writeRequest = mWriteRequests.ElementAt(0);

  // Step 4. Remove writeRequest from stream.[[writeRequests]].
  mWriteRequests.RemoveElementAt(0);

  // Step 5. Set stream.[[inFlightWriteRequest]] to writeRequest.
  mInFlightWriteRequest = writeRequest;
}

// https://streams.spec.whatwg.org/#writable-stream-reject-close-and-closed-promise-if-needed
void WritableStream::RejectCloseAndClosedPromiseIfNeeded() {
  // Step 1. Assert: stream.[[state]] is "errored".
  MOZ_ASSERT(mState == WriterState::Errored);

  JS::Rooted<JS::Value> storedError(RootingCx(), mStoredError);
  // Step 2. If stream.[[closeRequest]] is not undefined,
  if (mCloseRequest) {
    // Step 2.1. Assert: stream.[[inFlightCloseRequest]] is undefined.
    MOZ_ASSERT(!mInFlightCloseRequest);

    // Step 2.2. Reject stream.[[closeRequest]] with stream.[[storedError]].
    mCloseRequest->MaybeReject(storedError);

    // Step 2.3. Set stream.[[closeRequest]] to undefined.
    mCloseRequest = nullptr;
  }

  // Step 3. Let writer be stream.[[writer]].
  RefPtr<WritableStreamDefaultWriter> writer = mWriter;

  // Step 4. If writer is not undefined,
  if (writer) {
    // Step 4.1. Reject writer.[[closedPromise]] with stream.[[storedError]].
    RefPtr<Promise> closedPromise = writer->ClosedPromise();
    closedPromise->MaybeReject(storedError);

    // Step 4.2. Set writer.[[closedPromise]].[[PromiseIsHandled]] to true.
    closedPromise->SetSettledPromiseIsHandled();
  }
}

// https://streams.spec.whatwg.org/#writable-stream-start-erroring
void WritableStream::StartErroring(JSContext* aCx,
                                   JS::Handle<JS::Value> aReason,
                                   ErrorResult& aRv) {
  // Step 1. Assert: stream.[[storedError]] is undefined.
  MOZ_ASSERT(mStoredError.isUndefined());

  // Step 2. Assert: stream.[[state]] is "writable".
  MOZ_ASSERT(mState == WriterState::Writable);

  // Step 3. Let controller be stream.[[controller]].
  RefPtr<WritableStreamDefaultController> controller = mController;
  // Step 4. Assert: controller is not undefined.
  MOZ_ASSERT(controller);

  // Step 5. Set stream.[[state]] to "erroring".
  mState = WriterState::Erroring;

  // Step 6. Set stream.[[storedError]] to reason.
  mStoredError = aReason;

  // Step 7. Let writer be stream.[[writer]].
  RefPtr<WritableStreamDefaultWriter> writer = mWriter;
  // Step 8. If writer is not undefined, perform !
  // WritableStreamDefaultWriterEnsureReadyPromiseRejected(writer, reason).
  if (writer) {
    WritableStreamDefaultWriterEnsureReadyPromiseRejected(writer, aReason, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 9. If ! WritableStreamHasOperationMarkedInFlight(stream) is false
  // and controller.[[started]] is true,
  // perform !WritableStreamFinishErroring(stream).
  if (!HasOperationMarkedInFlight() && controller->Started()) {
    FinishErroring(aCx, aRv);
  }
}

// https://streams.spec.whatwg.org/#writable-stream-update-backpressure
void WritableStream::UpdateBackpressure(bool aBackpressure, ErrorResult& aRv) {
  // Step 1. Assert: stream.[[state]] is "writable".
  MOZ_ASSERT(mState == WriterState::Writable);
  // Step 2. Assert: ! WritableStreamCloseQueuedOrInFlight(stream) is false.
  MOZ_ASSERT(!CloseQueuedOrInFlight());

  // Step 3. Let writer be stream.[[writer]].
  RefPtr<WritableStreamDefaultWriter> writer = mWriter;

  // Step 4. If writer is not undefined and backpressure is not
  // stream.[[backpressure]],
  if (writer && aBackpressure != mBackpressure) {
    // Step 4.1. If backpressure is true, set writer.[[readyPromise]] to a new
    // promise.
    if (aBackpressure) {
      RefPtr<Promise> promise = Promise::Create(writer->GetParentObject(), aRv);
      if (aRv.Failed()) {
        return;
      }
      writer->SetReadyPromise(promise);
    } else {
      // Step 4.2. Otherwise,
      // Step 4.2.1. Assert: backpressure is false.
      // Step 4.2.2. Resolve writer.[[readyPromise]] with undefined.
      writer->ReadyPromise()->MaybeResolveWithUndefined();
    }
  }

  // Step 5. Set stream.[[backpressure]] to backpressure.
  mBackpressure = aBackpressure;
}

// https://streams.spec.whatwg.org/#ws-constructor
already_AddRefed<WritableStream> WritableStream::Constructor(
    const GlobalObject& aGlobal,
    const Optional<JS::Handle<JSObject*>>& aUnderlyingSink,
    const QueuingStrategy& aStrategy, ErrorResult& aRv) {
  // Step 1. If underlyingSink is missing, set it to null.
  JS::Rooted<JSObject*> underlyingSinkObj(
      aGlobal.Context(),
      aUnderlyingSink.WasPassed() ? aUnderlyingSink.Value() : nullptr);

  // Step 2. Let underlyingSinkDict be underlyingSink, converted to
  //         an IDL value of type UnderlyingSink.
  UnderlyingSink underlyingSinkDict;
  if (underlyingSinkObj) {
    JS::Rooted<JS::Value> objValue(aGlobal.Context(),
                                   JS::ObjectValue(*underlyingSinkObj));
    dom::BindingCallContext callCx(aGlobal.Context(),
                                   "WritableStream.constructor");
    aRv.MightThrowJSException();
    if (!underlyingSinkDict.Init(callCx, objValue)) {
      aRv.StealExceptionFromJSContext(aGlobal.Context());
      return nullptr;
    }
  }

  // Step 3. If underlyingSinkDict["type"] exists, throw a RangeError exception.
  if (!underlyingSinkDict.mType.isUndefined()) {
    aRv.ThrowRangeError("Implementation preserved member 'type'");
    return nullptr;
  }

  // Step 4. Perform ! InitializeWritableStream(this).
  RefPtr<WritableStream> writableStream = new WritableStream(aGlobal);

  // Step 5. Let sizeAlgorithm be ! ExtractSizeAlgorithm(strategy).
  //
  // Implementation Note: The specification demands that if the size doesn't
  // exist, we instead would provide an algorithm that returns 1. Instead, we
  // will teach callers that a missing callback should simply return 1, rather
  // than gin up a fake callback here.
  //
  // This decision may need to be revisited if the default action ever diverges
  // within the specification.
  RefPtr<QueuingStrategySize> sizeAlgorithm =
      aStrategy.mSize.WasPassed() ? &aStrategy.mSize.Value() : nullptr;

  // Step 6. Let highWaterMark be ? ExtractHighWaterMark(strategy, 1).
  double highWaterMark = ExtractHighWaterMark(aStrategy, 1, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 7. Perform ? SetUpWritableStreamDefaultControllerFromUnderlyingSink(
  // this, underlyingSink, underlyingSinkDict, highWaterMark, sizeAlgorithm).
  SetUpWritableStreamDefaultControllerFromUnderlyingSink(
      aGlobal.Context(), writableStream, underlyingSinkObj, underlyingSinkDict,
      highWaterMark, sizeAlgorithm, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return writableStream.forget();
}

// https://streams.spec.whatwg.org/#writable-stream-abort
already_AddRefed<Promise> WritableStreamAbort(JSContext* aCx,
                                              WritableStream* aStream,
                                              JS::Handle<JS::Value> aReason,
                                              ErrorResult& aRv) {
  // Step 1. If stream.[[state]] is "closed" or "errored", return a promise
  // resolved with undefined.
  if (aStream->State() == WritableStream::WriterState::Closed ||
      aStream->State() == WritableStream::WriterState::Errored) {
    RefPtr<Promise> promise = Promise::Create(aStream->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }

  // Step 2. Signal abort on stream.[[controller]].[[signal]] with reason.
  RefPtr<WritableStreamDefaultController> controller = aStream->Controller();
  controller->Signal()->SignalAbort(aReason);

  // Step 3. Let state be stream.[[state]].
  WritableStream::WriterState state = aStream->State();

  // Step 4. If state is "closed" or "errored", return a promise resolved with
  // undefined. Note: We re-check the state because signaling abort runs author
  // code and that might have changed the state.
  if (aStream->State() == WritableStream::WriterState::Closed ||
      aStream->State() == WritableStream::WriterState::Errored) {
    RefPtr<Promise> promise = Promise::Create(aStream->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }

  // Step 5. If stream.[[pendingAbortRequest]] is not undefined, return
  // stream.[[pendingAbortRequest]]'s promise.
  if (aStream->GetPendingAbortRequestPromise()) {
    RefPtr<Promise> promise = aStream->GetPendingAbortRequestPromise();
    return promise.forget();
  }

  // Step 6. Assert: state is "writable" or "erroring".
  MOZ_ASSERT(state == WritableStream::WriterState::Writable ||
             state == WritableStream::WriterState::Erroring);

  // Step 7. Let wasAlreadyErroring be false.
  bool wasAlreadyErroring = false;

  // Step 8. If state is "erroring",
  JS::Rooted<JS::Value> reason(aCx, aReason);
  if (state == WritableStream::WriterState::Erroring) {
    // Step 8.1. Set wasAlreadyErroring to true.
    wasAlreadyErroring = true;
    // Step 8.2. Set reason to undefined.
    reason.setUndefined();
  }

  // Step 9. Let promise be a new promise.
  RefPtr<Promise> promise = Promise::Create(aStream->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 10. Set stream.[[pendingAbortRequest]] to a new pending abort request
  // whose promise is promise, reason is reason, and was already erroring is
  // wasAlreadyErroring.
  aStream->SetPendingAbortRequest(promise, reason, wasAlreadyErroring);

  // Step 11. If wasAlreadyErroring is false, perform !
  // WritableStreamStartErroring(stream, reason).
  if (!wasAlreadyErroring) {
    aStream->StartErroring(aCx, reason, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  // Step 12. Return promise.
  return promise.forget();
}

// https://streams.spec.whatwg.org/#ws-abort
already_AddRefed<Promise> WritableStream::Abort(JSContext* aCx,
                                                JS::Handle<JS::Value> aReason,
                                                ErrorResult& aRv) {
  // Step 1. If ! IsWritableStreamLocked(this) is true, return a promise
  // rejected with a TypeError exception.
  if (Locked()) {
    RefPtr<Promise> promise = Promise::Create(GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    promise->MaybeRejectWithTypeError("Canceled Locked Stream");
    return promise.forget();
  }

  // Step 2. Return ! WritableStreamAbort(this, reason).
  RefPtr<WritableStream> thisRefPtr = this;
  return WritableStreamAbort(aCx, thisRefPtr, aReason, aRv);
}

// https://streams.spec.whatwg.org/#writable-stream-close
already_AddRefed<Promise> WritableStreamClose(JSContext* aCx,
                                              WritableStream* aStream,
                                              ErrorResult& aRv) {
  // Step 1. Let state be stream.[[state]].
  WritableStream::WriterState state = aStream->State();

  // Step 2. If state is "closed" or "errored", return a promise rejected with a
  // TypeError exception.
  if (state == WritableStream::WriterState::Closed ||
      state == WritableStream::WriterState::Errored) {
    RefPtr<Promise> promise = Promise::Create(aStream->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    promise->MaybeRejectWithTypeError(
        "Can not close stream after closing or error");
    return promise.forget();
  }

  // Step 3. Assert: state is "writable" or "erroring".
  MOZ_ASSERT(state == WritableStream::WriterState::Writable ||
             state == WritableStream::WriterState::Erroring);

  // Step 4. Assert: ! WritableStreamCloseQueuedOrInFlight(stream) is false.
  MOZ_ASSERT(!aStream->CloseQueuedOrInFlight());

  // Step 5. Let promise be a new promise.
  RefPtr<Promise> promise = Promise::Create(aStream->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 6. Set stream.[[closeRequest]] to promise.
  aStream->SetCloseRequest(promise);

  // Step 7. Let writer be stream.[[writer]].
  RefPtr<WritableStreamDefaultWriter> writer = aStream->GetWriter();

  // Step 8. If writer is not undefined, and stream.[[backpressure]] is true,
  // and state is "writable", resolve writer.[[readyPromise]] with undefined.
  if (writer && aStream->Backpressure() &&
      state == WritableStream::WriterState::Writable) {
    writer->ReadyPromise()->MaybeResolveWithUndefined();
  }

  // Step 9.
  // Perform ! WritableStreamDefaultControllerClose(stream.[[controller]]).
  RefPtr<WritableStreamDefaultController> controller = aStream->Controller();
  WritableStreamDefaultControllerClose(aCx, controller, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 10. Return promise.
  return promise.forget();
}

// https://streams.spec.whatwg.org/#ws-close
already_AddRefed<Promise> WritableStream::Close(JSContext* aCx,
                                                ErrorResult& aRv) {
  // Step 1. If ! IsWritableStreamLocked(this) is true, return a promise
  // rejected with a TypeError exception.
  if (Locked()) {
    RefPtr<Promise> promise = Promise::Create(GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    promise->MaybeRejectWithTypeError("Can not close locked stream");
    return promise.forget();
  }

  // Step 2. If ! WritableStreamCloseQueuedOrInFlight(this) is true, return a
  // promise rejected with a TypeError exception.
  if (CloseQueuedOrInFlight()) {
    RefPtr<Promise> promise = Promise::Create(GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    promise->MaybeRejectWithTypeError("Stream is already closing");
    return promise.forget();
  }

  // Step 3. Return ! WritableStreamClose(this).
  RefPtr<WritableStream> thisRefPtr = this;
  return WritableStreamClose(aCx, thisRefPtr, aRv);
}

// https://streams.spec.whatwg.org/#acquire-writable-stream-default-writer
already_AddRefed<WritableStreamDefaultWriter>
AcquireWritableStreamDefaultWriter(WritableStream* aStream, ErrorResult& aRv) {
  // Step 1. Let writer be a new WritableStreamDefaultWriter.
  RefPtr<WritableStreamDefaultWriter> writer =
      new WritableStreamDefaultWriter(aStream->GetParentObject());

  // Step 2. Perform ? SetUpWritableStreamDefaultWriter(writer, stream).
  SetUpWritableStreamDefaultWriter(writer, aStream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 3. Return writer.
  return writer.forget();
}

already_AddRefed<WritableStreamDefaultWriter> WritableStream::GetWriter(
    ErrorResult& aRv) {
  return AcquireWritableStreamDefaultWriter(this, aRv);
}

// https://streams.spec.whatwg.org/#writable-stream-add-write-request
already_AddRefed<Promise> WritableStreamAddWriteRequest(WritableStream* aStream,
                                                        ErrorResult& aRv) {
  // Step 1. Assert: ! IsWritableStreamLocked(stream) is true.
  MOZ_ASSERT(IsWritableStreamLocked(aStream));

  // Step 2. Assert: stream.[[state]] is "writable".
  MOZ_ASSERT(aStream->State() == WritableStream::WriterState::Writable);

  // Step 3. Let promise be a new promise.
  RefPtr<Promise> promise = Promise::Create(aStream->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 4. Append promise to stream.[[writeRequests]].
  aStream->AppendWriteRequest(promise);

  // Step 5. Return promise.
  return promise.forget();
}

}  // namespace mozilla::dom
