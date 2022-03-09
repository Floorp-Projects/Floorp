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
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamController.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/ReadableStreamDefaultControllerBinding.h"
#include "mozilla/dom/ReadableStreamDefaultReaderBinding.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION(ReadableStreamController, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ReadableStreamController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ReadableStreamController)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamController)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// Note: Using the individual macros vs NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE
// because I need to specificy a manual implementation of
// NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN.
NS_IMPL_CYCLE_COLLECTION_CLASS(ReadableStreamDefaultController)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ReadableStreamDefaultController)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCancelAlgorithm, mStrategySizeAlgorithm,
                                  mPullAlgorithm, mStream)
  tmp->mQueue.clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    ReadableStreamDefaultController, ReadableStreamController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCancelAlgorithm, mStrategySizeAlgorithm,
                                    mPullAlgorithm, mStream)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ReadableStreamDefaultController,
                                               ReadableStreamController)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  // Trace the associated queue.
  for (const auto& queueEntry : tmp->mQueue) {
    aCallbacks.Trace(&queueEntry->mValue, "mQueue.mValue", aClosure);
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(ReadableStreamDefaultController,
                         ReadableStreamController)
NS_IMPL_RELEASE_INHERITED(ReadableStreamDefaultController,
                          ReadableStreamController)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamDefaultController)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(ReadableStreamController)

ReadableStreamDefaultController::ReadableStreamDefaultController(
    nsIGlobalObject* aGlobal)
    : ReadableStreamController(aGlobal) {
  mozilla::HoldJSObjects(this);
}

ReadableStreamDefaultController::~ReadableStreamDefaultController() {
  // MG:XXX: LinkedLists are required to be empty at destruction, but it seems
  //         it is possible to have a controller be destructed while still
  //         having entries in its queue.
  //
  //         This needs to be verified as not indicating some other issue.
  mozilla::DropJSObjects(this);
  mQueue.clear();
}

JSObject* ReadableStreamDefaultController::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ReadableStreamDefaultController_Binding::Wrap(aCx, this, aGivenProto);
}

void ReadableStreamDefaultController::SetStream(ReadableStream* aStream) {
  mStream = aStream;
}

// https://streams.spec.whatwg.org/#readable-stream-default-controller-can-close-or-enqueue
static bool ReadableStreamDefaultControllerCanCloseOrEnqueue(
    ReadableStreamDefaultController* aController) {
  // Step 1. Let state be controller.[[stream]].[[state]].
  ReadableStream::ReaderState state = aController->GetStream()->State();

  // Step 2. If controller.[[closeRequested]] is false and state is "readable",
  // return true.
  // Step 3. Return false.
  return !aController->CloseRequested() &&
         state == ReadableStream::ReaderState::Readable;
}

enum class CloseOrEnqueue { Close, Enqueue };

// https://streams.spec.whatwg.org/#readable-stream-default-controller-can-close-or-enqueue
// This is a variant of ReadableStreamDefaultControllerCanCloseOrEnqueue
// that also throws when the function would return false to improve error
// messages.
static bool ReadableStreamDefaultControllerCanCloseOrEnqueueAndThrow(
    ReadableStreamDefaultController* aController,
    CloseOrEnqueue aCloseOrEnqueue, ErrorResult& aRv) {
  // Step 1. Let state be controller.[[stream]].[[state]].
  ReadableStream::ReaderState state = aController->GetStream()->State();

  nsCString prefix;
  if (aCloseOrEnqueue == CloseOrEnqueue::Close) {
    prefix = "Cannot close a readable stream that "_ns;
  } else {
    prefix = "Cannot enqueue into a readable stream that "_ns;
  }

  switch (state) {
    case ReadableStream::ReaderState::Readable:
      // Step 2. If controller.[[closeRequested]] is false and
      // state is "readable", return true.
      // Note: We don't error/check for [[closeRequest]] first, because
      // [[closedRequest]] is still true even after the state is "closed".
      // This doesn't cause any spec observable difference.
      if (!aController->CloseRequested()) {
        return true;
      }

      // Step 3. Return false.
      aRv.ThrowTypeError(prefix + "has already been requested to close."_ns);
      return false;

    case ReadableStream::ReaderState::Closed:
      aRv.ThrowTypeError(prefix + "is already closed."_ns);
      return false;

    case ReadableStream::ReaderState::Errored:
      aRv.ThrowTypeError(prefix + "has errored."_ns);
      return false;

    default:
      MOZ_ASSERT_UNREACHABLE("Unknown ReaderState");
      return false;
  }
}

static Nullable<double> ReadableStreamDefaultControllerGetDesiredSize(
    ReadableStreamDefaultController* aController) {
  ReadableStream::ReaderState state = aController->GetStream()->State();
  if (state == ReadableStream::ReaderState::Errored) {
    return nullptr;
  }

  if (state == ReadableStream::ReaderState::Closed) {
    return 0.0;
  }

  return aController->StrategyHWM() - aController->QueueTotalSize();
}

// https://streams.spec.whatwg.org/#rs-default-controller-desired-size
Nullable<double> ReadableStreamDefaultController::GetDesiredSize() {
  // Step 1.
  return ReadableStreamDefaultControllerGetDesiredSize(this);
}

// https://streams.spec.whatwg.org/#readable-stream-default-controller-clear-algorithms
//
// Note: nullptr is used to indicate we run the default algorithm at the
//       moment,
//       so the below doesn't quite match the spec, but serves the correct
//       purpose for disconnecting the algorithms from the object graph to allow
//       collection.
//
//       As far as I know, this isn't currently visible, but we need to keep
//       this in mind. This is a weakness of this current implementation, and
//       I'd prefer to have a better answer here eventually.
void ReadableStreamDefaultControllerClearAlgorithms(
    ReadableStreamDefaultController* aController) {
  // Step 1.
  aController->SetPullAlgorithm(nullptr);

  // Step 2.
  aController->SetCancelAlgorithm(nullptr);

  // Step 3.
  aController->setStrategySizeAlgorithm(nullptr);
}

// https://streams.spec.whatwg.org/#readable-stream-default-controller-close
void ReadableStreamDefaultControllerClose(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    ErrorResult& aRv) {
  // Step 1.
  if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(aController)) {
    return;
  }

  // Step 2.
  RefPtr<ReadableStream> stream = aController->GetStream();

  // Step 3.
  aController->SetCloseRequested(true);

  // Step 4.
  if (aController->Queue().isEmpty()) {
    // Step 4.1
    ReadableStreamDefaultControllerClearAlgorithms(aController);

    // Step 4.2
    ReadableStreamClose(aCx, stream, aRv);
  }
}

// https://streams.spec.whatwg.org/#rs-default-controller-close
void ReadableStreamDefaultController::Close(JSContext* aCx, ErrorResult& aRv) {
  // Step 1.
  if (!ReadableStreamDefaultControllerCanCloseOrEnqueueAndThrow(
          this, CloseOrEnqueue::Close, aRv)) {
    return;
  }

  // Step 2.
  ReadableStreamDefaultControllerClose(aCx, this, aRv);
}

MOZ_CAN_RUN_SCRIPT static void ReadableStreamDefaultControllerCallPullIfNeeded(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    ErrorResult& aRv);

// https://streams.spec.whatwg.org/#readable-stream-default-controller-enqueue
void ReadableStreamDefaultControllerEnqueue(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    JS::Handle<JS::Value> aChunk, ErrorResult& aRv) {
  // Step 1.
  if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(aController)) {
    return;
  }

  // Step 2.
  RefPtr<ReadableStream> stream = aController->GetStream();

  // Step 3.
  if (IsReadableStreamLocked(stream) &&
      ReadableStreamGetNumReadRequests(stream) > 0) {
    ReadableStreamFulfillReadRequest(aCx, stream, aChunk, false, aRv);
  } else {
    // Step 4.1
    Optional<JS::Handle<JS::Value>> optionalChunk(aCx, aChunk);

    // Step 4.3 (Re-ordered);
    RefPtr<QueuingStrategySize> sizeAlgorithm(
        aController->StrategySizeAlgorithm());

    // If !sizeAlgorithm, we return 1, which is inlined from
    // https://streams.spec.whatwg.org/#make-size-algorithm-from-size-function
    double chunkSize =
        sizeAlgorithm
            ? sizeAlgorithm->Call(
                  optionalChunk, aRv,
                  "ReadableStreamDefaultController.[[strategySizeAlgorithm]]",
                  CallbackObject::eRethrowExceptions)
            : 1.0;

    // If this is an uncatchable exception we can't continue.
    if (aRv.IsUncatchableException()) {
      return;
    }

    // Step 4.2:
    if (aRv.MaybeSetPendingException(
            aCx, "ReadableStreamDefaultController.enqueue")) {
      JS::RootedValue errorValue(aCx);

      JS_GetPendingException(aCx, &errorValue);

      // Step 4.2.1

      ReadableStreamDefaultControllerError(aCx, aController, errorValue, aRv);
      if (aRv.Failed()) {
        return;
      }

      // Step 4.2.2 Caller must treat aRv as if it were a completion
      // value
      aRv.MightThrowJSException();
      aRv.ThrowJSException(aCx, errorValue);
      return;
    }

    // Step 4.4
    EnqueueValueWithSize(aController, aChunk, chunkSize, aRv);

    // Step 4.5
    // Note we convert the pending exception to a JS value here, and then
    // re-throw it because we save this exception and re-expose it elsewhere
    // and there are tests to ensure the identity of these errors are the same.
    if (aRv.MaybeSetPendingException(
            aCx, "ReadableStreamDefaultController.enqueue")) {
      JS::RootedValue errorValue(aCx);

      JS_GetPendingException(aCx, &errorValue);

      // Step 4.5.1
      ReadableStreamDefaultControllerError(aCx, aController, errorValue, aRv);
      if (aRv.Failed()) {
        return;
      }

      // Step 4.5.2 Caller must treat aRv as if it were a completion
      // value
      aRv.MightThrowJSException();
      aRv.ThrowJSException(aCx, errorValue);
      return;
    }
  }

  // Step 5.
  ReadableStreamDefaultControllerCallPullIfNeeded(aCx, aController, aRv);
}

// https://streams.spec.whatwg.org/#rs-default-controller-close
void ReadableStreamDefaultController::Enqueue(JSContext* aCx,
                                              JS::Handle<JS::Value> aChunk,
                                              ErrorResult& aRv) {
  // Step 1.
  if (!ReadableStreamDefaultControllerCanCloseOrEnqueueAndThrow(
          this, CloseOrEnqueue::Enqueue, aRv)) {
    return;
  }

  // Step 2.
  ReadableStreamDefaultControllerEnqueue(aCx, this, aChunk, aRv);
}

void ReadableStreamDefaultController::Error(JSContext* aCx,
                                            JS::Handle<JS::Value> aError,
                                            ErrorResult& aRv) {
  ReadableStreamDefaultControllerError(aCx, this, aError, aRv);
}

// https://streams.spec.whatwg.org/#readable-stream-default-controller-should-call-pull
static bool ReadableStreamDefaultControllerShouldCallPull(
    ReadableStreamDefaultController* aController) {
  // Step 1.
  ReadableStream* stream = aController->GetStream();

  // Step 2.
  if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(aController)) {
    return false;
  }

  // Step 3.
  if (!aController->Started()) {
    return false;
  }

  // Step 4.
  if (IsReadableStreamLocked(stream) &&
      ReadableStreamGetNumReadRequests(stream) > 0) {
    return true;
  }

  // Step 5.
  Nullable<double> desiredSize =
      ReadableStreamDefaultControllerGetDesiredSize(aController);

  // Step 6.
  MOZ_ASSERT(!desiredSize.IsNull());

  // Step 7 + 8
  return desiredSize.Value() > 0;
}

// https://streams.spec.whatwg.org/#readable-stream-default-controller-error
void ReadableStreamDefaultControllerError(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  // Step 1.
  ReadableStream* stream = aController->GetStream();

  // Step 2.
  if (stream->State() != ReadableStream::ReaderState::Readable) {
    return;
  }

  // Step 3.
  ResetQueue(aController);

  // Step 4.
  ReadableStreamDefaultControllerClearAlgorithms(aController);

  // Step 5.
  ReadableStreamError(aCx, stream, aValue, aRv);
}

// MG:XXX: Probably can find base class between this and
// StartPromiseNativeHandler
class PullIfNeededNativePromiseHandler final : public PromiseNativeHandler {
  ~PullIfNeededNativePromiseHandler() override = default;

  // Virtually const, but cycle collected
  RefPtr<ReadableStreamDefaultController> mController;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PullIfNeededNativePromiseHandler)

  explicit PullIfNeededNativePromiseHandler(
      ReadableStreamDefaultController* aController)
      : PromiseNativeHandler(), mController(aController) {}

  MOZ_CAN_RUN_SCRIPT void ResolvedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#readable-stream-default-controller-call-pull-if-needed
    // Step 7.1
    mController->SetPulling(false);
    // Step 7.2
    if (mController->PullAgain()) {
      // Step 7.2.1
      mController->SetPullAgain(false);

      // Step 7.2.2
      ErrorResult rv;
      ReadableStreamDefaultControllerCallPullIfNeeded(
          aCx, MOZ_KnownLive(mController), aRv);
    }
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#readable-stream-default-controller-call-pull-if-needed
    // Step 8.1
    ReadableStreamDefaultControllerError(aCx, mController, aValue, aRv);
  }
};

// Cycle collection methods for promise handler.
NS_IMPL_CYCLE_COLLECTION(PullIfNeededNativePromiseHandler, mController)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PullIfNeededNativePromiseHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PullIfNeededNativePromiseHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PullIfNeededNativePromiseHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#readable-stream-default-controller-call-pull-if-needed
static void ReadableStreamDefaultControllerCallPullIfNeeded(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    ErrorResult& aRv) {
  // Step 1.
  bool shouldPull = ReadableStreamDefaultControllerShouldCallPull(aController);

  // Step 2.
  if (!shouldPull) {
    return;
  }

  // Step 3.
  if (aController->Pulling()) {
    // Step 3.1
    aController->SetPullAgain(true);
    // Step 3.2
    return;
  }

  // Step 4.
  MOZ_ASSERT(!aController->PullAgain());

  // Step 5.
  aController->SetPulling(true);

  // Step 6.
  RefPtr<UnderlyingSourcePullCallbackHelper> pullAlgorithm(
      aController->GetPullAlgorithm());

  RefPtr<Promise> pullPromise =
      pullAlgorithm ? pullAlgorithm->PullCallback(aCx, *aController, aRv)
                    : Promise::CreateResolvedWithUndefined(
                          aController->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 7 + 8:
  pullPromise->AppendNativeHandler(
      new PullIfNeededNativePromiseHandler(aController));
}

class StartPromiseNativeHandler final : public PromiseNativeHandler {
  ~StartPromiseNativeHandler() override = default;

  RefPtr<ReadableStreamDefaultController> mController;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(StartPromiseNativeHandler)

  explicit StartPromiseNativeHandler(
      ReadableStreamDefaultController* aController)
      : PromiseNativeHandler(), mController(aController) {}

  MOZ_CAN_RUN_SCRIPT
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    MOZ_ASSERT(mController);

    // https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller
    //
    // Step 11.1
    mController->SetStarted(true);

    // Step 11.2
    mController->SetPulling(false);

    // Step 11.3
    mController->SetPullAgain(false);

    // Step 11.4:

    RefPtr<ReadableStreamDefaultController> stackController = mController;
    ReadableStreamDefaultControllerCallPullIfNeeded(aCx, stackController, aRv);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller
    // Step 12.1
    ReadableStreamDefaultControllerError(aCx, mController, aValue, aRv);
  }
};

// Cycle collection methods for promise handler
NS_IMPL_CYCLE_COLLECTION(StartPromiseNativeHandler, mController)
NS_IMPL_CYCLE_COLLECTING_ADDREF(StartPromiseNativeHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(StartPromiseNativeHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StartPromiseNativeHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller
void SetUpReadableStreamDefaultController(
    JSContext* aCx, ReadableStream* aStream,
    ReadableStreamDefaultController* aController,
    UnderlyingSourceStartCallbackHelper* aStartAlgorithm,
    UnderlyingSourcePullCallbackHelper* aPullAlgorithm,
    UnderlyingSourceCancelCallbackHelper* aCancelAlgorithm,
    double aHighWaterMark, QueuingStrategySize* aSizeAlgorithm,
    ErrorResult& aRv) {
  // Step 1.
  MOZ_ASSERT(!aStream->Controller());

  // Step 2.
  aController->SetStream(aStream);

  // Step 3.
  ResetQueue(aController);

  // Step 4.
  aController->SetStarted(false);
  aController->SetCloseRequested(false);
  aController->SetPullAgain(false);
  aController->SetPulling(false);

  // Step 5.
  aController->setStrategySizeAlgorithm(aSizeAlgorithm);
  aController->SetStrategyHWM(aHighWaterMark);

  // Step 6.
  aController->SetPullAlgorithm(aPullAlgorithm);

  // Step 7.
  aController->SetCancelAlgorithm(aCancelAlgorithm);

  // Step 8.
  aStream->SetController(aController);

  // Step 9. Default algorithm returns undefined. See Step 2 of
  // https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller
  JS::RootedValue startResult(aCx, JS::UndefinedValue());
  if (aStartAlgorithm) {
    // Strong Refs:
    RefPtr<UnderlyingSourceStartCallbackHelper> startAlgorithm(aStartAlgorithm);
    RefPtr<ReadableStreamDefaultController> controller(aController);

    startAlgorithm->StartCallback(aCx, *controller, &startResult, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 10.
  RefPtr<Promise> startPromise = Promise::Create(GetIncumbentGlobal(), aRv);
  if (aRv.Failed()) {
    return;
  }
  startPromise->MaybeResolve(startResult);

  // Step 11 & 12:
  RefPtr<StartPromiseNativeHandler> startPromiseHandler =
      new StartPromiseNativeHandler(aController);

  startPromise->AppendNativeHandler(startPromiseHandler);
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
void SetupReadableStreamDefaultControllerFromUnderlyingSource(
    JSContext* aCx, ReadableStream* aStream, JS::HandleObject aUnderlyingSource,
    UnderlyingSource& aUnderlyingSourceDict, double aHighWaterMark,
    QueuingStrategySize* aSizeAlgorithm, ErrorResult& aRv) {
  // Step 1.
  RefPtr<ReadableStreamDefaultController> controller =
      new ReadableStreamDefaultController(aStream->GetParentObject());

  // Step 2+5: Similar to ReadableStream's sizeAlgorithm, until i can figure
  // out a better way, going to devolve undefined return to caller.
  RefPtr<UnderlyingSourceStartCallbackHelper> startAlgorithm =
      aUnderlyingSourceDict.mStart.WasPassed()
          ? new UnderlyingSourceStartCallbackHelper(
                aUnderlyingSourceDict.mStart.Value(), aUnderlyingSource)
          : nullptr;

  // Step 3+6: Same as above, except Promise<Undefined>
  RefPtr<UnderlyingSourcePullCallbackHelper> pullAlgorithm =
      aUnderlyingSourceDict.mPull.WasPassed()
          ? new IDLUnderlyingSourcePullCallbackHelper(
                aUnderlyingSourceDict.mPull.Value(), aUnderlyingSource)
          : nullptr;

  // Step 4+7: Same as above:
  RefPtr<UnderlyingSourceCancelCallbackHelper> cancelAlgorithm =
      aUnderlyingSourceDict.mCancel.WasPassed()
          ? new IDLUnderlyingSourceCancelCallbackHelper(
                aUnderlyingSourceDict.mCancel.Value(), aUnderlyingSource)
          : nullptr;

  // Step 8:
  SetUpReadableStreamDefaultController(aCx, aStream, controller, startAlgorithm,
                                       pullAlgorithm, cancelAlgorithm,
                                       aHighWaterMark, aSizeAlgorithm, aRv);
}

// https://streams.spec.whatwg.org/#rs-default-controller-private-cancel
already_AddRefed<Promise> ReadableStreamDefaultController::CancelSteps(
    JSContext* aCx, JS::Handle<JS::Value> aReason, ErrorResult& aRv) {
  // Step 1.
  ResetQueue(this);

  // Step 2.
  Optional<JS::Handle<JS::Value>> errorOption(aCx, aReason);
  RefPtr<UnderlyingSourceCancelCallbackHelper> callback =
      this->GetCancelAlgorithm();
  RefPtr<Promise> result =
      callback ? callback->CancelCallback(aCx, errorOption, aRv)
               : Promise::CreateResolvedWithUndefined(GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 3.
  ReadableStreamDefaultControllerClearAlgorithms(this);

  // Step 4.
  return result.forget();
}

// https://streams.spec.whatwg.org/#rs-default-controller-private-pull
void ReadableStreamDefaultController::PullSteps(JSContext* aCx,
                                                ReadRequest* aReadRequest,
                                                ErrorResult& aRv) {
  // Step 1.
  RefPtr<ReadableStream> stream = mStream;

  // Step 2.
  if (!mQueue.isEmpty()) {
    // Step 2.1
    JS::RootedValue chunk(aCx);
    DequeueValue(this, &chunk);

    // Step 2.2
    if (CloseRequested() && mQueue.isEmpty()) {
      // Step 2.2.1
      ReadableStreamDefaultControllerClearAlgorithms(this);
      // Step 2.2.2
      ReadableStreamClose(aCx, stream, aRv);
      if (aRv.Failed()) {
        return;
      }
    } else {
      // Step 2.3
      ReadableStreamDefaultControllerCallPullIfNeeded(aCx, this, aRv);
      if (aRv.Failed()) {
        return;
      }
    }

    // Step 2.4
    aReadRequest->ChunkSteps(aCx, chunk, aRv);
  } else {
    // Step 3.
    // Step 3.1
    ReadableStreamAddReadRequest(stream, aReadRequest);
    // Step 3.2
    ReadableStreamDefaultControllerCallPullIfNeeded(aCx, this, aRv);
  }
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultcontroller-releasesteps
void ReadableStreamDefaultController::ReleaseSteps() {
  // Step 1. Return.
}

}  // namespace mozilla::dom
