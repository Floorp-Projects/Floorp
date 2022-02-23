/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Exception.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/AbortSignal.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/WritableStreamDefaultController.h"
#include "mozilla/dom/WritableStreamDefaultControllerBinding.h"
// #include "mozilla/dom/ReadableStreamDefaultReaderBinding.h"
#include "mozilla/dom/UnderlyingSinkBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsISupports.h"

namespace mozilla::dom {

// Note: Using the individual macros vs NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE
// because I need to specificy a manual implementation of
// NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN.
NS_IMPL_CYCLE_COLLECTION_CLASS(WritableStreamDefaultController)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WritableStreamDefaultController)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal, mSignal, mStrategySizeAlgorithm,
                                  mWriteAlgorithm, mCloseAlgorithm,
                                  mAbortAlgorithm, mStream)
  tmp->mQueue.clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WritableStreamDefaultController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal, mSignal, mStrategySizeAlgorithm,
                                    mWriteAlgorithm, mCloseAlgorithm,
                                    mAbortAlgorithm, mStream)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WritableStreamDefaultController)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  // Trace the associated queue.
  for (const auto& queueEntry : tmp->mQueue) {
    aCallbacks.Trace(&queueEntry->mValue, "mQueue.mValue", aClosure);
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WritableStreamDefaultController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WritableStreamDefaultController)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WritableStreamDefaultController)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

WritableStreamDefaultController::WritableStreamDefaultController(
    nsISupports* aGlobal, WritableStream& aStream)
    : mGlobal(do_QueryInterface(aGlobal)), mStream(&aStream) {}

WritableStreamDefaultController::~WritableStreamDefaultController() {
  // MG:XXX: LinkedLists are required to be empty at destruction, but it seems
  //         it is possible to have a controller be destructed while still
  //         having entries in its queue.
  //
  //         This needs to be verified as not indicating some other issue.
  mQueue.clear();
}

JSObject* WritableStreamDefaultController::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return WritableStreamDefaultController_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#ws-default-controller-error
void WritableStreamDefaultController::Error(JSContext* aCx,
                                            JS::Handle<JS::Value> aError,
                                            ErrorResult& aRv) {
  // Step 1. Let state be this.[[stream]].[[state]].
  // Step 2. If state is not "writable", return.
  if (mStream->State() != WritableStream::WriterState::Writable) {
    return;
  }
  // Step 3. Perform ! WritableStreamDefaultControllerError(this, e).
  RefPtr<WritableStreamDefaultController> thisRefPtr = this;
  WritableStreamDefaultControllerError(aCx, thisRefPtr, aError, aRv);
}

// https://streams.spec.whatwg.org/#ws-default-controller-private-abort
already_AddRefed<Promise> WritableStreamDefaultController::AbortSteps(
    JSContext* aCx, JS::Handle<JS::Value> aReason, ErrorResult& aRv) {
  // Step 1. Let result be the result of performing this.[[abortAlgorithm]],
  // passing reason.
  RefPtr<UnderlyingSinkAbortCallbackHelper> abortAlgorithm(mAbortAlgorithm);
  Optional<JS::Handle<JS::Value>> optionalReason(aCx, aReason);
  RefPtr<Promise> abortPromise =
      abortAlgorithm
          ? abortAlgorithm->AbortCallback(aCx, optionalReason, aRv)
          : Promise::CreateResolvedWithUndefined(GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 2. Perform ! WritableStreamDefaultControllerClearAlgorithms(this).
  ClearAlgorithms();

  // Step 3. Return result.
  return abortPromise.forget();
}

// https://streams.spec.whatwg.org/#ws-default-controller-private-error
void WritableStreamDefaultController::ErrorSteps() {
  // Step 1. Perform ! ResetQueue(this).
  ResetQueue(this);
}

void WritableStreamDefaultController::SetSignal(AbortSignal* aSignal) {
  MOZ_ASSERT(aSignal);
  mSignal = aSignal;
}

MOZ_CAN_RUN_SCRIPT static void
WritableStreamDefaultControllerAdvanceQueueIfNeeded(
    JSContext* aCx, WritableStreamDefaultController* aController,
    ErrorResult& aRv);

class WritableStartPromiseNativeHandler final : public PromiseNativeHandler {
  ~WritableStartPromiseNativeHandler() = default;

  // Virtually const, but cycle collected
  RefPtr<WritableStreamDefaultController> mController;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(WritableStartPromiseNativeHandler)

  explicit WritableStartPromiseNativeHandler(
      WritableStreamDefaultController* aController)
      : PromiseNativeHandler(), mController(aController) {}

  MOZ_CAN_RUN_SCRIPT void ResolvedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller
    // Step 17. Upon fulfillment of startPromise,
    // Step 17.1. Assert: stream.[[state]] is "writable" or "erroring".
    MOZ_ASSERT(mController->Stream()->State() ==
                   WritableStream::WriterState::Writable ||
               mController->Stream()->State() ==
                   WritableStream::WriterState::Erroring);
    // Step 17.2. Set controller.[[started]] to true.
    mController->SetStarted(true);
    // Step 17.3 Perform
    // ! WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller).
    WritableStreamDefaultControllerAdvanceQueueIfNeeded(
        aCx, MOZ_KnownLive(mController), aRv);
  }

  MOZ_CAN_RUN_SCRIPT void RejectedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller
    RefPtr<WritableStream> stream = mController->Stream();
    // Step 18. Upon rejection of startPromise with reason r,
    // Step 18.1. Assert: stream.[[state]] is "writable" or "erroring".
    MOZ_ASSERT(stream->State() == WritableStream::WriterState::Writable ||
               stream->State() == WritableStream::WriterState::Erroring);
    // Step 18.2. Set controller.[[started]] to true.
    mController->SetStarted(true);
    // Step 18.3. Perform ! WritableStreamDealWithRejection(stream, r).
    stream->DealWithRejection(aCx, aValue, aRv);
  }
};

NS_IMPL_CYCLE_COLLECTION(WritableStartPromiseNativeHandler, mController)
NS_IMPL_CYCLE_COLLECTING_ADDREF(WritableStartPromiseNativeHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WritableStartPromiseNativeHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WritableStartPromiseNativeHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller
void SetUpWritableStreamDefaultController(
    JSContext* aCx, WritableStream* aStream,
    WritableStreamDefaultController* aController,
    UnderlyingSinkStartCallbackHelper* aStartAlgorithm,
    UnderlyingSinkWriteCallbackHelper* aWriteAlgorithm,
    UnderlyingSinkCloseCallbackHelper* aCloseAlgorithm,
    UnderlyingSinkAbortCallbackHelper* aAbortAlgorithm, double aHighWaterMark,
    QueuingStrategySize* aSizeAlgorithm, ErrorResult& aRv) {
  // Step 1. Assert: stream implements WritableStream.
  // Step 2. Assert: stream.[[controller]] is undefined.
  MOZ_ASSERT(!aStream->Controller());

  // Step 3. Set controller.[[stream]] to stream.
  // Note: Already set in
  // SetUpWritableStreamDefaultControllerFromUnderlyingSink.
  MOZ_ASSERT(aController->Stream() == aStream);

  // Step 4. Set stream.[[controller]] to controller.
  aStream->SetController(aController);

  // Step 5. Perform ! ResetQueue(controller).
  ResetQueue(aController);

  // Step 6. Set controller.[[signal]] to a new AbortSignal.
  RefPtr<AbortSignal> signal = new AbortSignal(aController->GetParentObject(),
                                               false, JS::UndefinedHandleValue);
  aController->SetSignal(signal);

  // Step 7. Set controller.[[started]] to false.
  aController->SetStarted(false);

  // Step 8. Set controller.[[strategySizeAlgorithm]] to sizeAlgorithm.
  aController->SetStrategySizeAlgorithm(aSizeAlgorithm);

  // Step 9. Set controller.[[strategyHWM]] to highWaterMark.
  aController->SetStrategyHWM(aHighWaterMark);

  // Step 10. Set controller.[[writeAlgorithm]] to writeAlgorithm.
  aController->SetWriteAlgorithm(aWriteAlgorithm);

  // Step 11. Set controller.[[closeAlgorithm]] to closeAlgorithm.
  aController->SetCloseAlgorithm(aCloseAlgorithm);

  // Step 12. Set controller.[[abortAlgorithm]] to abortAlgorithm.
  aController->SetAbortAlgorithm(aAbortAlgorithm);

  // Step 13. Let backpressure be !
  // WritableStreamDefaultControllerGetBackpressure(controller).
  bool backpressure = aController->GetBackpressure();

  // Step 14. Perform ! WritableStreamUpdateBackpressure(stream, backpressure).
  aStream->UpdateBackpressure(backpressure, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 15. Let startResult be the result of performing startAlgorithm. (This
  // may throw an exception.)
  JS::Rooted<JS::Value> startResult(aCx, JS::UndefinedValue());
  if (aStartAlgorithm) {
    // Strong Refs:
    RefPtr<UnderlyingSinkStartCallbackHelper> startAlgorithm(aStartAlgorithm);
    RefPtr<WritableStreamDefaultController> controller(aController);

    startAlgorithm->StartCallback(aCx, *controller, &startResult, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 16. Let startPromise be a promise resolved with startResult.
  RefPtr<Promise> startPromise = Promise::Create(GetIncumbentGlobal(), aRv);
  if (aRv.Failed()) {
    return;
  }
  startPromise->MaybeResolve(startResult);

  // Step 17/18.
  RefPtr<WritableStartPromiseNativeHandler> startPromiseHandler =
      new WritableStartPromiseNativeHandler(aController);
  startPromise->AppendNativeHandler(startPromiseHandler);
}

// https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller-from-underlying-sink
void SetUpWritableStreamDefaultControllerFromUnderlyingSink(
    JSContext* aCx, WritableStream* aStream, JS::HandleObject aUnderlyingSink,
    UnderlyingSink& aUnderlyingSinkDict, double aHighWaterMark,
    QueuingStrategySize* aSizeAlgorithm, ErrorResult& aRv) {
  // Step 1.
  RefPtr<WritableStreamDefaultController> controller =
      new WritableStreamDefaultController(aStream->GetParentObject(), *aStream);

  // Step 6. (implicit Step 2.)
  RefPtr<UnderlyingSinkStartCallbackHelper> startAlgorithm =
      aUnderlyingSinkDict.mStart.WasPassed()
          ? new UnderlyingSinkStartCallbackHelper(
                aUnderlyingSinkDict.mStart.Value(), aUnderlyingSink)
          : nullptr;

  // Step 7. (implicit Step 3.)
  RefPtr<UnderlyingSinkWriteCallbackHelper> writeAlgorithm =
      aUnderlyingSinkDict.mWrite.WasPassed()
          ? new UnderlyingSinkWriteCallbackHelper(
                aUnderlyingSinkDict.mWrite.Value(), aUnderlyingSink)
          : nullptr;

  // Step 8. (implicit Step 4.)
  RefPtr<UnderlyingSinkCloseCallbackHelper> closeAlgorithm =
      aUnderlyingSinkDict.mClose.WasPassed()
          ? new UnderlyingSinkCloseCallbackHelper(
                aUnderlyingSinkDict.mClose.Value(), aUnderlyingSink)
          : nullptr;

  // Step 9. (implicit Step 5.)
  RefPtr<UnderlyingSinkAbortCallbackHelper> abortAlgorithm =
      aUnderlyingSinkDict.mAbort.WasPassed()
          ? new UnderlyingSinkAbortCallbackHelper(
                aUnderlyingSinkDict.mAbort.Value(), aUnderlyingSink)
          : nullptr;

  // Step 10.
  SetUpWritableStreamDefaultController(
      aCx, aStream, controller, startAlgorithm, writeAlgorithm, closeAlgorithm,
      abortAlgorithm, aHighWaterMark, aSizeAlgorithm, aRv);
}

// MG:XXX: Probably can find base class between this and
// StartPromiseNativeHandler
class SinkCloseNativePromiseHandler final : public PromiseNativeHandler {
  ~SinkCloseNativePromiseHandler() = default;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(SinkCloseNativePromiseHandler)

  explicit SinkCloseNativePromiseHandler(
      WritableStreamDefaultController* aController)
      : PromiseNativeHandler(), mController(aController) {}

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#writable-stream-default-controller-process-close
    RefPtr<WritableStream> stream = mController->Stream();
    // Step 7. Upon fulfillment of sinkClosePromise,
    // Step 7.1. Perform ! WritableStreamFinishInFlightClose(stream).
    stream->FinishInFlightClose();
  }

  MOZ_CAN_RUN_SCRIPT void RejectedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#writable-stream-default-controller-process-close
    RefPtr<WritableStream> stream = mController->Stream();
    // Step 8. Upon rejection of sinkClosePromise with reason reason,
    // Step 8.1. Perform ! WritableStreamFinishInFlightCloseWithError(stream,
    // reason).

    stream->FinishInFlightCloseWithError(aCx, aValue, aRv);
  }

 private:
  RefPtr<WritableStreamDefaultController> mController;
};

// Cycle collection methods for promise handler.
NS_IMPL_CYCLE_COLLECTION(SinkCloseNativePromiseHandler, mController)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SinkCloseNativePromiseHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SinkCloseNativePromiseHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SinkCloseNativePromiseHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#writable-stream-default-controller-process-close
MOZ_CAN_RUN_SCRIPT static void WritableStreamDefaultControllerProcessClose(
    JSContext* aCx, WritableStreamDefaultController* aController,
    ErrorResult& aRv) {
  // Step 1. Let stream be controller.[[stream]].
  RefPtr<WritableStream> stream = aController->Stream();

  // Step 2. Perform ! WritableStreamMarkCloseRequestInFlight(stream).
  stream->MarkCloseRequestInFlight();

  // Step 3. Perform ! DequeueValue(controller).
  JS::Rooted<JS::Value> value(aCx);
  DequeueValue(aController, &value);

  // Step 4. Assert: controller.[[queue]] is empty.
  MOZ_ASSERT(aController->Queue().isEmpty());

  // Step 5. Let sinkClosePromise be the result of performing
  // controller.[[closeAlgorithm]].
  RefPtr<UnderlyingSinkCloseCallbackHelper> closeAlgorithm(
      aController->GetCloseAlgorithm());

  RefPtr<Promise> sinkClosePromise =
      closeAlgorithm ? closeAlgorithm->CloseCallback(aCx, aRv)
                     : Promise::CreateResolvedWithUndefined(
                           aController->GetParentObject(), aRv);

  if (aRv.Failed()) {
    return;
  }

  // Step 6. Perform !
  // WritableStreamDefaultControllerClearAlgorithms(controller).
  aController->ClearAlgorithms();

  // Step 7 + 8.
  sinkClosePromise->AppendNativeHandler(
      new SinkCloseNativePromiseHandler(aController));
}

// MG:XXX: Probably can find base class between this and
// StartPromiseNativeHandler
class SinkWriteNativePromiseHandler final : public PromiseNativeHandler {
  ~SinkWriteNativePromiseHandler() = default;

  // Virtually const, but is cycle collected
  RefPtr<WritableStreamDefaultController> mController;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(SinkWriteNativePromiseHandler)

  explicit SinkWriteNativePromiseHandler(
      WritableStreamDefaultController* aController)
      : PromiseNativeHandler(), mController(aController) {}

  MOZ_CAN_RUN_SCRIPT void ResolvedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#writable-stream-default-controller-process-write
    RefPtr<WritableStream> stream = mController->Stream();

    // Step 4.1. Perform ! WritableStreamFinishInFlightWrite(stream).
    stream->FinishInFlightWrite();

    // Step 4.2. Let state be stream.[[state]].
    WritableStream::WriterState state = stream->State();

    // Step 4.3. Assert: state is "writable" or "erroring".
    MOZ_ASSERT(state == WritableStream::WriterState::Writable ||
               state == WritableStream::WriterState::Erroring);

    // Step 4.4. Perform ! DequeueValue(controller).
    JS::Rooted<JS::Value> value(aCx);
    DequeueValue(mController, &value);

    // Step 4.5. If ! WritableStreamCloseQueuedOrInFlight(stream) is false and
    // state is "writable",
    if (!stream->CloseQueuedOrInFlight() &&
        state == WritableStream::WriterState::Writable) {
      // Step 4.5.1. Let backpressure be !
      // WritableStreamDefaultControllerGetBackpressure(controller).
      bool backpressure = mController->GetBackpressure();
      // Step 4.5.2. Perform ! WritableStreamUpdateBackpressure(stream,
      // backpressure).
      stream->UpdateBackpressure(backpressure, aRv);
      if (aRv.Failed()) {
        return;
      }
    }

    // Step 4.6. Perform !
    // WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller).
    WritableStreamDefaultControllerAdvanceQueueIfNeeded(
        aCx, MOZ_KnownLive(mController), aRv);
  }

  MOZ_CAN_RUN_SCRIPT void RejectedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#writable-stream-default-controller-process-write
    RefPtr<WritableStream> stream = mController->Stream();

    // Step 5.1. If stream.[[state]] is "writable", perform !
    // WritableStreamDefaultControllerClearAlgorithms(controller).
    if (stream->State() == WritableStream::WriterState::Writable) {
      mController->ClearAlgorithms();
    }

    // Step 5.2. Perform ! WritableStreamFinishInFlightWriteWithError(stream,
    // reason)

    stream->FinishInFlightWriteWithError(aCx, aValue, aRv);
  }
};

// Cycle collection methods for promise handler.
NS_IMPL_CYCLE_COLLECTION(SinkWriteNativePromiseHandler, mController)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SinkWriteNativePromiseHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SinkWriteNativePromiseHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SinkWriteNativePromiseHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#writable-stream-default-controller-process-write
MOZ_CAN_RUN_SCRIPT static void WritableStreamDefaultControllerProcessWrite(
    JSContext* aCx, WritableStreamDefaultController* aController,
    JS::Handle<JS::Value> aChunk, ErrorResult& aRv) {
  // Step 1. Let stream be controller.[[stream]].
  RefPtr<WritableStream> stream = aController->Stream();

  // Step 2. Perform ! WritableStreamMarkFirstWriteRequestInFlight(stream).
  stream->MarkFirstWriteRequestInFlight();

  // Step 3. Let sinkWritePromise be the result of performing
  // controller.[[writeAlgorithm]], passing in chunk.
  RefPtr<UnderlyingSinkWriteCallbackHelper> writeAlgorithm(
      aController->GetWriteAlgorithm());

  RefPtr<Promise> sinkWritePromise =
      writeAlgorithm
          ? writeAlgorithm->WriteCallback(aCx, aChunk, *aController, aRv)
          : Promise::CreateResolvedWithUndefined(aController->GetParentObject(),
                                                 aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 4 + 5:
  sinkWritePromise->AppendNativeHandler(
      new SinkWriteNativePromiseHandler(aController));
}

// We use a JS::MagicValue to represent the close sentinel required by the spec.
// Normal JavaScript code can not generate magic values, so we can use this
// as a special value. However care has to be taken to not leak the magic value
// to other code.
constexpr JSWhyMagic CLOSE_SENTINEL = JS_GENERIC_MAGIC;

// https://streams.spec.whatwg.org/#writable-stream-default-controller-advance-queue-if-needed
static void WritableStreamDefaultControllerAdvanceQueueIfNeeded(
    JSContext* aCx, WritableStreamDefaultController* aController,
    ErrorResult& aRv) {
  // Step 1. Let stream be controller.[[stream]].
  RefPtr<WritableStream> stream = aController->Stream();

  // Step 2. If controller.[[started]] is false, return.
  if (!aController->Started()) {
    return;
  }

  // Step 3. If stream.[[inFlightWriteRequest]] is not undefined, return.
  if (stream->GetInFlightWriteRequest()) {
    return;
  }

  // Step 4. Let state be stream.[[state]].
  WritableStream::WriterState state = stream->State();

  // Step 5. Assert: state is not "closed" or "errored".
  MOZ_ASSERT(state != WritableStream::WriterState::Closed &&
             state != WritableStream::WriterState::Errored);

  // Step 6. If state is "erroring",
  if (state == WritableStream::WriterState::Erroring) {
    // Step 6.1. Perform ! WritableStreamFinishErroring(stream).
    stream->FinishErroring(aCx, aRv);

    // Step 6.2. Return.
    return;
  }

  // Step 7. If controller.[[queue]] is empty, return.
  if (aController->Queue().isEmpty()) {
    return;
  }

  // Step 8. Let value be ! PeekQueueValue(controller).
  JS::Rooted<JS::Value> value(aCx);
  PeekQueueValue(aController, &value);

  // Step 9. If value is the close sentinel, perform !
  // WritableStreamDefaultControllerProcessClose(controller).
  if (value.isMagic(CLOSE_SENTINEL)) {
    WritableStreamDefaultControllerProcessClose(aCx, aController, aRv);
    return;
  }

  // Step 10. Otherwise, perform !
  // WritableStreamDefaultControllerProcessWrite(controller, value).
  WritableStreamDefaultControllerProcessWrite(aCx, aController, value, aRv);
}

// https://streams.spec.whatwg.org/#writable-stream-default-controller-close
void WritableStreamDefaultControllerClose(
    JSContext* aCx, WritableStreamDefaultController* aController,
    ErrorResult& aRv) {
  // Step 1. Perform ! EnqueueValueWithSize(controller, close sentinel, 0).
  JS::Rooted<JS::Value> aCloseSentinel(aCx, JS::MagicValue(CLOSE_SENTINEL));
  EnqueueValueWithSize(aController, aCloseSentinel, 0, aRv);
  MOZ_ASSERT(!aRv.Failed());

  // Step 2. Perform !
  // WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller).
  WritableStreamDefaultControllerAdvanceQueueIfNeeded(aCx, aController, aRv);
}

// https://streams.spec.whatwg.org/#writable-stream-default-controller-write
void WritableStreamDefaultControllerWrite(
    JSContext* aCx, WritableStreamDefaultController* aController,
    JS::Handle<JS::Value> aChunk, double chunkSize, ErrorResult& aRv) {
  // Step 1. Let enqueueResult be EnqueueValueWithSize(controller, chunk,
  // chunkSize).
  IgnoredErrorResult rv;
  EnqueueValueWithSize(aController, aChunk, chunkSize, rv);

  // Step 2. If enqueueResult is an abrupt completion,
  if (rv.MaybeSetPendingException(aCx,
                                  "WritableStreamDefaultController.write")) {
    JS::Rooted<JS::Value> error(aCx);
    JS_GetPendingException(aCx, &error);
    JS_ClearPendingException(aCx);

    // Step 2.1. Perform !
    // WritableStreamDefaultControllerErrorIfNeeded(controller,
    // enqueueResult.[[Value]]).
    WritableStreamDefaultControllerErrorIfNeeded(aCx, aController, error, aRv);

    // Step 2.2. Return.
    return;
  }

  // Step 3. Let stream be controller.[[stream]].
  RefPtr<WritableStream> stream = aController->Stream();

  // Step 4. If ! WritableStreamCloseQueuedOrInFlight(stream) is false and
  // stream.[[state]] is "writable",
  if (!stream->CloseQueuedOrInFlight() &&
      stream->State() == WritableStream::WriterState::Writable) {
    // Step 4.1. Let backpressure be
    // !WritableStreamDefaultControllerGetBackpressure(controller).
    bool backpressure = aController->GetBackpressure();

    // Step 4.2. Perform ! WritableStreamUpdateBackpressure(stream,
    // backpressure).
    stream->UpdateBackpressure(backpressure, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 5. Perform
  // ! WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller).
  WritableStreamDefaultControllerAdvanceQueueIfNeeded(aCx, aController, aRv);
}

void WritableStreamDefaultControllerError(
    JSContext* aCx, WritableStreamDefaultController* aController,
    JS::Handle<JS::Value> aError, ErrorResult& aRv) {
  // Step 1. Let stream be controller.[[stream]].
  RefPtr<WritableStream> stream = aController->Stream();

  // Step 2. Assert: stream.[[state]] is "writable".
  MOZ_ASSERT(stream->State() == WritableStream::WriterState::Writable);

  // Step 3. Perform
  // ! WritableStreamDefaultControllerClearAlgorithms(controller).
  aController->ClearAlgorithms();

  // Step 4.Perform ! WritableStreamStartErroring(stream, error).
  stream->StartErroring(aCx, aError, aRv);
}

// https://streams.spec.whatwg.org/#writable-stream-default-controller-error-if-needed
void WritableStreamDefaultControllerErrorIfNeeded(
    JSContext* aCx, WritableStreamDefaultController* aController,
    JS::Handle<JS::Value> aError, ErrorResult& aRv) {
  // Step 1. If controller.[[stream]].[[state]] is "writable", perform
  // !WritableStreamDefaultControllerError(controller, error).
  if (aController->Stream()->State() == WritableStream::WriterState::Writable) {
    WritableStreamDefaultControllerError(aCx, aController, aError, aRv);
  }
}

// https://streams.spec.whatwg.org/#writable-stream-default-controller-get-chunk-size
double WritableStreamDefaultControllerGetChunkSize(
    JSContext* aCx, WritableStreamDefaultController* aController,
    JS::Handle<JS::Value> aChunk, ErrorResult& aRv) {
  // Step 1. Let returnValue be the result of performing
  // controller.[[strategySizeAlgorithm]], passing in chunk, and interpreting
  // the result as a completion record.
  RefPtr<QueuingStrategySize> sizeAlgorithm(
      aController->StrategySizeAlgorithm());

  // If !sizeAlgorithm, we return 1, which is inlined from
  // https://streams.spec.whatwg.org/#make-size-algorithm-from-size-function
  Optional<JS::Handle<JS::Value>> optionalChunk(aCx, aChunk);

  double chunkSize =
      sizeAlgorithm
          ? sizeAlgorithm->Call(
                optionalChunk, aRv,
                "WritableStreamDefaultController.[[strategySizeAlgorithm]]",
                CallbackObject::eRethrowExceptions)
          : 1.0;

  // Step 2. If returnValue is an abrupt completion,
  if (aRv.MaybeSetPendingException(
          aCx, "WritableStreamDefaultController.[[strategySizeAlgorithm]]")) {
    JS::Rooted<JS::Value> error(aCx);
    JS_GetPendingException(aCx, &error);
    JS_ClearPendingException(aCx);

    // Step 2.1. Perform !
    // WritableStreamDefaultControllerErrorIfNeeded(controller,
    // returnValue.[[Value]]).
    WritableStreamDefaultControllerErrorIfNeeded(aCx, aController, error, aRv);

    // Step 2.2. Return 1.
    return 1.0;
  }

  // Step 3. Return returnValue.[[Value]].
  return chunkSize;
}

}  // namespace mozilla::dom
