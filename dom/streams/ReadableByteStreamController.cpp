/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ArrayBuffer.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/ValueArray.h"
#include "js/experimental/TypedData.h"
#include "js/friend/ErrorMessages.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ByteStreamHelpers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ReadableByteStreamController.h"
#include "mozilla/dom/ReadableByteStreamControllerBinding.h"
#include "mozilla/dom/ReadIntoRequest.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamBYOBReader.h"
#include "mozilla/dom/ReadableStreamBYOBRequest.h"
#include "mozilla/dom/ReadableStreamController.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/dom/ReadableStreamGenericReader.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"

#include <algorithm>  // std::min

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(ReadableByteStreamController)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ReadableByteStreamController,
                                                ReadableStreamController)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mByobRequest, mCancelAlgorithm,
                                  mPullAlgorithm, mStream)
  tmp->ClearPendingPullIntos();
  tmp->ClearQueue();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ReadableByteStreamController,
                                                  ReadableStreamController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mByobRequest, mCancelAlgorithm,
                                    mPullAlgorithm, mStream)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ReadableByteStreamController,
                                               ReadableStreamController)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  // Trace the associated queue + list
  for (const auto& queueEntry : tmp->mQueue) {
    aCallbacks.Trace(&queueEntry->mBuffer, "mQueue.mBuffer", aClosure);
  }
  for (const auto& pullInto : tmp->mPendingPullIntos) {
    aCallbacks.Trace(&pullInto->mBuffer, "mPendingPullIntos.mBuffer", aClosure);
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(ReadableByteStreamController, ReadableStreamController)
NS_IMPL_RELEASE_INHERITED(ReadableByteStreamController,
                          ReadableStreamController)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableByteStreamController)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(ReadableStreamController)

ReadableByteStreamController::ReadableByteStreamController(
    nsIGlobalObject* aGlobal)
    : ReadableStreamController(aGlobal) {
  mozilla::HoldJSObjects(this);
}

ReadableByteStreamController::~ReadableByteStreamController() {
  ClearPendingPullIntos();
  ClearQueue();
  mozilla::DropJSObjects(this);
}

void ReadableByteStreamController::ClearQueue() {
  // Since the pull intos are traced only by the owning
  // ReadableByteStreamController, when clearning the list we also clear JS
  // references to avoid dangling JS references.
  for (auto* queueEntry : mQueue) {
    queueEntry->ClearBuffer();
  }
  mQueue.clear();
}

void ReadableByteStreamController::ClearPendingPullIntos() {
  // Since the pull intos are traced only by the owning
  // ReadableByteStreamController, when clearning the list we also clear JS
  // references to avoid dangling JS references.
  for (auto* pullInto : mPendingPullIntos) {
    pullInto->ClearBuffer();
  }
  mPendingPullIntos.clear();
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollergetbyobrequest
already_AddRefed<ReadableStreamBYOBRequest>
ReadableByteStreamControllerGetBYOBRequest(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv) {
  // Step 1.
  if (!aController->GetByobRequest() &&
      !aController->PendingPullIntos().isEmpty()) {
    // Step 1.1:
    PullIntoDescriptor* firstDescriptor =
        aController->PendingPullIntos().getFirst();

    // Step 1.2:
    aRv.MightThrowJSException();
    JS::Rooted<JSObject*> buffer(aCx, firstDescriptor->Buffer());
    JS::Rooted<JSObject*> view(
        aCx, JS_NewUint8ArrayWithBuffer(
                 aCx, buffer,
                 firstDescriptor->ByteOffset() + firstDescriptor->BytesFilled(),
                 int64_t(firstDescriptor->ByteLength() -
                         firstDescriptor->BytesFilled())));
    if (!view) {
      aRv.StealExceptionFromJSContext(aCx);
      return nullptr;
    }

    // Step 1.3:
    RefPtr<ReadableStreamBYOBRequest> byobRequest =
        new ReadableStreamBYOBRequest(aController->GetParentObject());

    // Step 1.4:
    byobRequest->SetController(aController);

    // Step 1.5:
    byobRequest->SetView(view);

    // Step 1.6:
    aController->SetByobRequest(byobRequest);
  }

  // Step 2.
  RefPtr<ReadableStreamBYOBRequest> request(aController->GetByobRequest());
  return request.forget();
}

already_AddRefed<ReadableStreamBYOBRequest>
ReadableByteStreamController::GetByobRequest(JSContext* aCx, ErrorResult& aRv) {
  return ReadableByteStreamControllerGetBYOBRequest(aCx, this, aRv);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-get-desired-size
Nullable<double> ReadableByteStreamControllerGetDesiredSize(
    const ReadableByteStreamController* aController) {
  // Step 1.
  ReadableStream::ReaderState state = aController->Stream()->State();

  // Step 2.
  if (state == ReadableStream::ReaderState::Errored) {
    return nullptr;
  }

  // Step 3.
  if (state == ReadableStream::ReaderState::Closed) {
    return 0.0;
  }

  // Step 4.
  return aController->StrategyHWM() - aController->QueueTotalSize();
}

Nullable<double> ReadableByteStreamController::GetDesiredSize() const {
  // Step 1.
  return ReadableByteStreamControllerGetDesiredSize(this);
}

JSObject* ReadableByteStreamController::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ReadableByteStreamController_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-invalidate-byob-request
static void ReadableByteStreamControllerInvalidateBYOBRequest(
    ReadableByteStreamController* aController) {
  // Step 1.
  if (!aController->GetByobRequest()) {
    return;
  }

  // Step 2.
  aController->GetByobRequest()->SetController(nullptr);

  // Step 3.
  aController->GetByobRequest()->SetView(nullptr);

  // Step 4.
  aController->SetByobRequest(nullptr);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-clear-pending-pull-intos
void ReadableByteStreamControllerClearPendingPullIntos(
    ReadableByteStreamController* aController) {
  // Step 1.
  ReadableByteStreamControllerInvalidateBYOBRequest(aController);

  // Step 2.
  aController->ClearPendingPullIntos();
}

// https://streams.spec.whatwg.org/#reset-queue
void ResetQueue(ReadableByteStreamController* aContainer) {
  // Step 1. Implied by type.
  // Step 2.
  aContainer->ClearQueue();

  // Step 3.
  aContainer->SetQueueTotalSize(0);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-clear-algorithms
void ReadableByteStreamControllerClearAlgorithms(
    ReadableByteStreamController* aController) {
  // Step 1. Set controller.[[pullAlgorithm]] to undefined.
  aController->SetPullAlgorithm(nullptr);

  // Step 2. Set controller.[[cancelAlgorithm]] to undefined.
  aController->SetCancelAlgorithm(nullptr);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-error
void ReadableByteStreamControllerError(
    ReadableByteStreamController* aController, JS::Handle<JS::Value> aValue,
    ErrorResult& aRv) {
  // Step 1. Let stream be controller.[[stream]].
  ReadableStream* stream = aController->Stream();

  // Step 2. If stream.[[state]] is not "readable", return.
  if (stream->State() != ReadableStream::ReaderState::Readable) {
    return;
  }

  // Step 3. Perform
  // !ReadableByteStreamControllerClearPendingPullIntos(controller).
  ReadableByteStreamControllerClearPendingPullIntos(aController);

  // Step 4. Perform !ResetQueue(controller).
  ResetQueue(aController);

  // Step 5. Perform !ReadableByteStreamControllerClearAlgorithms(controller).
  ReadableByteStreamControllerClearAlgorithms(aController);

  // Step 6. Perform !ReadableStreamError(stream, e).
  AutoJSAPI jsapi;
  if (!jsapi.Init(aController->GetParentObject())) {
    return;
  }
  ReadableStreamError(jsapi.cx(), stream, aValue, aRv);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-close
void ReadableByteStreamControllerClose(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv) {
  // Step 1.
  RefPtr<ReadableStream> stream = aController->Stream();

  // Step 2.
  if (aController->CloseRequested() ||
      stream->State() != ReadableStream::ReaderState::Readable) {
    return;
  }

  // Step 3.
  if (aController->QueueTotalSize() > 0) {
    // Step 3.1
    aController->SetCloseRequested(true);
    // Step 3.2
    return;
  }

  // Step 4.
  if (!aController->PendingPullIntos().isEmpty()) {
    // Step 4.1
    PullIntoDescriptor* firstPendingPullInto =
        aController->PendingPullIntos().getFirst();
    // Step 4.2
    if (firstPendingPullInto->BytesFilled() > 0) {
      // Step 4.2.1
      ErrorResult rv;
      rv.ThrowTypeError("Leftover Bytes");

      JS::RootedValue exception(aCx);
      MOZ_ALWAYS_TRUE(ToJSValue(aCx, std::move(rv), &exception));

      // Step 4.2.2
      ReadableByteStreamControllerError(aController, exception, aRv);
      if (aRv.Failed()) {
        return;
      }

      aRv.MightThrowJSException();
      aRv.ThrowJSException(aCx, exception);
      return;
    }
  }

  // Step 5.
  ReadableByteStreamControllerClearAlgorithms(aController);

  // Step 6.
  ReadableStreamClose(aCx, stream, aRv);
}

// https://streams.spec.whatwg.org/#rbs-controller-close
void ReadableByteStreamController::Close(JSContext* aCx, ErrorResult& aRv) {
  // Step 1.
  if (mCloseRequested) {
    aRv.ThrowTypeError("Close already requested");
    return;
  }

  // Step 2.
  if (Stream()->State() != ReadableStream::ReaderState::Readable) {
    aRv.ThrowTypeError("Closing un-readable stream controller");
    return;
  }

  // Step 3.
  ReadableByteStreamControllerClose(aCx, this, aRv);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-enqueue-chunk-to-queue
void ReadableByteStreamControllerEnqueueChunkToQueue(
    ReadableByteStreamController* aController,
    JS::Handle<JSObject*> aTransferredBuffer, size_t aByteOffset,
    size_t aByteLength) {
  // Step 1.
  RefPtr<ReadableByteStreamQueueEntry> queueEntry =
      new ReadableByteStreamQueueEntry(aTransferredBuffer, aByteOffset,
                                       aByteLength);
  aController->Queue().insertBack(queueEntry);

  // Step 2.
  aController->AddToQueueTotalSize(double(aByteLength));
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollerenqueueclonedchunktoqueue
void ReadableByteStreamControllerEnqueueClonedChunkToQueue(
    JSContext* aCx, ReadableByteStreamController* aController,
    JS::Handle<JSObject*> aBuffer, size_t aByteOffset, size_t aByteLength,
    ErrorResult& aRv) {
  // Step 1. Let cloneResult be CloneArrayBuffer(buffer, byteOffset, byteLength,
  // %ArrayBuffer%).
  aRv.MightThrowJSException();
  JS::Rooted<JSObject*> cloneResult(
      aCx, JS::ArrayBufferClone(aCx, aBuffer, aByteOffset, aByteLength));

  // Step 2. If cloneResult is an abrupt completion,
  if (!cloneResult) {
    JS::Rooted<JS::Value> exception(aCx);
    if (!JS_GetPendingException(aCx, &exception)) {
      // Uncatchable exception; we should mark aRv and return.
      aRv.StealExceptionFromJSContext(aCx);
      return;
    }
    JS_ClearPendingException(aCx);

    // Step 2.1. Perform ! ReadableByteStreamControllerError(controller,
    // cloneResult.[[Value]]).
    ReadableByteStreamControllerError(aController, exception, aRv);
    if (aRv.Failed()) {
      return;
    }

    // Step 2.2. Return cloneResult.
    aRv.ThrowJSException(aCx, exception);
    return;
  }

  // Step 3. Perform !
  // ReadableByteStreamControllerEnqueueChunkToQueue(controller,
  // cloneResult.[[Value]], 0, byteLength).
  ReadableByteStreamControllerEnqueueChunkToQueue(aController, cloneResult, 0,
                                                  aByteLength);
}

already_AddRefed<PullIntoDescriptor>
ReadableByteStreamControllerShiftPendingPullInto(
    ReadableByteStreamController* aController);

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollerenqueuedetachedpullintotoqueue
void ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(
    JSContext* aCx, ReadableByteStreamController* aController,
    PullIntoDescriptor* aPullIntoDescriptor, ErrorResult& aRv) {
  // Step 1. Assert: pullIntoDescriptor’s reader type is "none".
  MOZ_ASSERT(aPullIntoDescriptor->GetReaderType() == ReaderType::None);

  // Step 2. If pullIntoDescriptor’s bytes filled > 0,
  // perform ? ReadableByteStreamControllerEnqueueClonedChunkToQueue(controller,
  // pullIntoDescriptor’s buffer, pullIntoDescriptor’s byte offset,
  // pullIntoDescriptor’s bytes filled).
  if (aPullIntoDescriptor->BytesFilled() > 0) {
    JS::Rooted<JSObject*> buffer(aCx, aPullIntoDescriptor->Buffer());
    ReadableByteStreamControllerEnqueueClonedChunkToQueue(
        aCx, aController, buffer, aPullIntoDescriptor->ByteOffset(),
        aPullIntoDescriptor->BytesFilled(), aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 3. Perform !
  // ReadableByteStreamControllerShiftPendingPullInto(controller).
  RefPtr<PullIntoDescriptor> discarded =
      ReadableByteStreamControllerShiftPendingPullInto(aController);
  (void)discarded;
}

// https://streams.spec.whatwg.org/#readable-stream-get-num-read-into-requests
static size_t ReadableStreamGetNumReadIntoRequests(ReadableStream* aStream) {
  // Step 1.
  MOZ_ASSERT(ReadableStreamHasBYOBReader(aStream));

  // Step 2.
  return aStream->GetReader()->AsBYOB()->ReadIntoRequests().length();
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-should-call-pull
bool ReadableByteStreamControllerShouldCallPull(
    ReadableByteStreamController* aController) {
  // Step 1.
  ReadableStream* stream = aController->Stream();

  // Step 2.
  if (stream->State() != ReadableStream::ReaderState::Readable) {
    return false;
  }

  // Step 3.
  if (aController->CloseRequested()) {
    return false;
  }

  // Step 4.
  if (!aController->Started()) {
    return false;
  }

  // Step 5.
  if (ReadableStreamHasDefaultReader(stream) &&
      ReadableStreamGetNumReadRequests(stream) > 0) {
    return true;
  }

  // Step 6.
  if (ReadableStreamHasBYOBReader(stream) &&
      ReadableStreamGetNumReadIntoRequests(stream) > 0) {
    return true;
  }

  // Step 7.
  Nullable<double> desiredSize =
      ReadableByteStreamControllerGetDesiredSize(aController);

  // Step 8.
  MOZ_ASSERT(!desiredSize.IsNull());

  // Step 9.
  if (desiredSize.Value() > 0) {
    return true;
  }

  // Step 10.
  return false;
}

// MG:XXX: There's a template hiding here for handling the difference between
// default and byte stream, eventually?
class ByteStreamPullIfNeededPromiseHandler final : public PromiseNativeHandler {
  ~ByteStreamPullIfNeededPromiseHandler() = default;

  // Virtually const, but cycle collected
  RefPtr<ReadableByteStreamController> mController;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ByteStreamPullIfNeededPromiseHandler)

  explicit ByteStreamPullIfNeededPromiseHandler(
      ReadableByteStreamController* aController)
      : PromiseNativeHandler(), mController(aController) {}

  MOZ_CAN_RUN_SCRIPT
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#readable-byte-stream-controller-call-pull-if-needed
    // Step 7.1
    mController->SetPulling(false);
    // Step 7.2
    if (mController->PullAgain()) {
      // Step 7.2.1
      mController->SetPullAgain(false);

      // Step 7.2.2
      ReadableByteStreamControllerCallPullIfNeeded(
          aCx, MOZ_KnownLive(mController), aRv);
    }
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#readable-byte-stream-controller-call-pull-if-needed
    // Step 8.1
    ReadableByteStreamControllerError(mController, aValue, aRv);
  }
};

// Cycle collection methods for promise handler.
NS_IMPL_CYCLE_COLLECTION(ByteStreamPullIfNeededPromiseHandler, mController)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ByteStreamPullIfNeededPromiseHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ByteStreamPullIfNeededPromiseHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ByteStreamPullIfNeededPromiseHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-call-pull-if-needed
void ReadableByteStreamControllerCallPullIfNeeded(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv) {
  // Step 1.
  bool shouldPull = ReadableByteStreamControllerShouldCallPull(aController);

  // Step 2.
  if (!shouldPull) {
    return;
  }

  // Step 3.
  if (aController->Pulling()) {
    aController->SetPullAgain(true);
    return;
  }

  // Step 4.
  MOZ_ASSERT(!aController->PullAgain());

  // Step 5.
  aController->SetPulling(true);

  // Step 6.
  RefPtr<ReadableStreamController> controller(aController);
  RefPtr<UnderlyingSourcePullCallbackHelper> pullAlgorithm(
      aController->GetPullAlgorithm());
  RefPtr<Promise> pullPromise =
      pullAlgorithm ? pullAlgorithm->PullCallback(aCx, *controller, aRv)
                    : Promise::CreateResolvedWithUndefined(
                          controller->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return;
  }

  // Steps 7+8
  RefPtr<ByteStreamPullIfNeededPromiseHandler> promiseHandler =
      new ByteStreamPullIfNeededPromiseHandler(aController);
  pullPromise->AppendNativeHandler(promiseHandler);
}

bool ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(
    JSContext* aCx, ReadableByteStreamController* aController,
    PullIntoDescriptor* aPullIntoDescriptor, ErrorResult& aRv);

JSObject* ReadableByteStreamControllerConvertPullIntoDescriptor(
    JSContext* aCx, PullIntoDescriptor* pullIntoDescriptor, ErrorResult& aRv);

// https://streams.spec.whatwg.org/#readable-stream-fulfill-read-into-request
MOZ_CAN_RUN_SCRIPT
void ReadableStreamFulfillReadIntoRequest(JSContext* aCx,
                                          ReadableStream* aStream,
                                          JS::HandleValue aChunk, bool done,
                                          ErrorResult& aRv) {
  // Step 1. Assert: !ReadableStreamHasBYOBReader(stream) is true.
  MOZ_ASSERT(ReadableStreamHasBYOBReader(aStream));

  // Step 2. Let reader be stream.[[reader]].
  ReadableStreamBYOBReader* reader = aStream->GetReader()->AsBYOB();

  // Step 3. Assert: reader.[[readIntoRequests]] is not empty.
  MOZ_ASSERT(!reader->ReadIntoRequests().isEmpty());

  // Step 4. Let readIntoRequest be reader.[[readIntoRequests]][0].
  // Step 5. Remove readIntoRequest from reader.[[readIntoRequests]].
  RefPtr<ReadIntoRequest> readIntoRequest =
      reader->ReadIntoRequests().popFirst();

  // Step 6. If done is true, perform readIntoRequest’s close steps, given
  // chunk.
  if (done) {
    readIntoRequest->CloseSteps(aCx, aChunk, aRv);
    return;
  }

  // Step 7. Otherwise, perform readIntoRequest’s chunk steps, given chunk.
  readIntoRequest->ChunkSteps(aCx, aChunk, aRv);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-commit-pull-into-descriptor
MOZ_CAN_RUN_SCRIPT
void ReadableByteStreamControllerCommitPullIntoDescriptor(
    JSContext* aCx, ReadableStream* aStream,
    PullIntoDescriptor* pullIntoDescriptor, ErrorResult& aRv) {
  // Step 1. Assert: stream.[[state]] is not "errored".
  MOZ_ASSERT(aStream->State() != ReadableStream::ReaderState::Errored);

  // Step 2. Assert: pullIntoDescriptor.reader type is not "none".
  MOZ_ASSERT(pullIntoDescriptor->GetReaderType() != ReaderType::None);

  // Step 3. Let done be false.
  bool done = false;

  // Step 4. If stream.[[state]] is "closed",
  if (aStream->State() == ReadableStream::ReaderState::Closed) {
    // Step 4.1. Assert: pullIntoDescriptor’s bytes filled is 0.
    MOZ_ASSERT(pullIntoDescriptor->BytesFilled() == 0);

    // Step 4.2. Set done to true.
    done = true;
  }

  // Step 5. Let filledView be !
  // ReadableByteStreamControllerConvertPullIntoDescriptor(pullIntoDescriptor).
  JS::RootedObject filledView(
      aCx, ReadableByteStreamControllerConvertPullIntoDescriptor(
               aCx, pullIntoDescriptor, aRv));
  if (aRv.Failed()) {
    return;
  }
  JS::RootedValue filledViewValue(aCx, JS::ObjectValue(*filledView));

  // Step 6. If pullIntoDescriptor’s reader type is "default",
  if (pullIntoDescriptor->GetReaderType() == ReaderType::Default) {
    // Step 6.1. Perform !ReadableStreamFulfillReadRequest(stream, filledView,
    // done).
    ReadableStreamFulfillReadRequest(aCx, aStream, filledViewValue, done, aRv);
    return;
  }

  // Step 7.1. Assert: pullIntoDescriptor’s reader type is "byob".
  MOZ_ASSERT(pullIntoDescriptor->GetReaderType() == ReaderType::BYOB);

  // Step 7.2 Perform !ReadableStreamFulfillReadIntoRequest(stream, filledView,
  // done).
  ReadableStreamFulfillReadIntoRequest(aCx, aStream, filledViewValue, done,
                                       aRv);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-process-pull-into-descriptors-using-queue
MOZ_CAN_RUN_SCRIPT
void ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv) {
  // Step 1. Assert: controller.[[closeRequested]] is false.
  MOZ_ASSERT(!aController->CloseRequested());

  // Step 2. While controller.[[pendingPullIntos]] is not empty,
  while (!aController->PendingPullIntos().isEmpty()) {
    // Step 2.1 If controller.[[queueTotalSize]] is 0, return.
    if (aController->QueueTotalSize() == 0) {
      return;
    }

    // Step 2.2. Let pullIntoDescriptor be controller.[[pendingPullIntos]][0].
    RefPtr<PullIntoDescriptor> pullIntoDescriptor =
        aController->PendingPullIntos().getFirst();

    // Step 2.3. If
    // !ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(controller,
    // pullIntoDescriptor) is true,
    bool ready = ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(
        aCx, aController, pullIntoDescriptor, aRv);
    if (aRv.Failed()) {
      return;
    }

    if (ready) {
      //  Step 2.3.1. Perform
      //  !ReadableByteStreamControllerShiftPendingPullInto(controller).
      RefPtr<PullIntoDescriptor> discardedPullIntoDescriptor =
          ReadableByteStreamControllerShiftPendingPullInto(aController);

      // Step 2.3.2. Perform
      // !ReadableByteStreamControllerCommitPullIntoDescriptor(controller.[[stream]],
      //         pullIntoDescriptor).
      RefPtr<ReadableStream> stream(aController->Stream());
      ReadableByteStreamControllerCommitPullIntoDescriptor(
          aCx, stream, pullIntoDescriptor, aRv);
      if (aRv.Failed()) {
        return;
      }
    }
  }
}

MOZ_CAN_RUN_SCRIPT
void ReadableByteStreamControllerHandleQueueDrain(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv);

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollerfillreadrequestfromqueue
MOZ_CAN_RUN_SCRIPT void ReadableByteStreamControllerFillReadRequestFromQueue(
    JSContext* aCx, ReadableByteStreamController* aController,
    ReadRequest* aReadRequest, ErrorResult& aRv) {
  // Step 1. Assert: controller.[[queueTotalSize]] > 0.
  MOZ_ASSERT(aController->QueueTotalSize() > 0);

  // Step 2. Let entry be controller.[[queue]][0].
  // Step 3. Remove entry from controller.[[queue]].
  RefPtr<ReadableByteStreamQueueEntry> entry = aController->Queue().popFirst();

  // Step 4. Set controller.[[queueTotalSize]] to controller.[[queueTotalSize]]
  // − entry’s byte length.
  aController->SetQueueTotalSize(aController->QueueTotalSize() -
                                 double(entry->ByteLength()));

  // Step 5. Perform ! ReadableByteStreamControllerHandleQueueDrain(controller).
  ReadableByteStreamControllerHandleQueueDrain(aCx, aController, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 6. Let view be ! Construct(%Uint8Array%, « entry’s buffer, entry’s
  // byte offset, entry’s byte length »).
  aRv.MightThrowJSException();
  JS::Rooted<JSObject*> buffer(aCx, entry->Buffer());
  JS::Rooted<JSObject*> view(
      aCx, JS_NewUint8ArrayWithBuffer(aCx, buffer, entry->ByteOffset(),
                                      int64_t(entry->ByteLength())));
  if (!view) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  // Step 7. Perform readRequest’s chunk steps, given view.
  JS::Rooted<JS::Value> viewValue(aCx, JS::ObjectValue(*view));
  aReadRequest->ChunkSteps(aCx, viewValue, aRv);
}

MOZ_CAN_RUN_SCRIPT void
ReadableByteStreamControllerProcessReadRequestsUsingQueue(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv) {
  // Step 1. Let reader be controller.[[stream]].[[reader]].
  // Step 2. Assert: reader implements ReadableStreamDefaultReader.
  RefPtr<ReadableStreamDefaultReader> reader =
      aController->Stream()->GetDefaultReader();

  // Step 3. While reader.[[readRequests]] is not empty,
  while (!reader->ReadRequests().isEmpty()) {
    // Step 3.1. If controller.[[queueTotalSize]] is 0, return.
    if (aController->QueueTotalSize() == 0) {
      return;
    }

    // Step 3.2. Let readRequest be reader.[[readRequests]][0].
    // Step 3.3. Remove readRequest from reader.[[readRequests]].
    RefPtr<ReadRequest> readRequest = reader->ReadRequests().popFirst();

    // Step 3.4. Perform !
    // ReadableByteStreamControllerFillReadRequestFromQueue(controller,
    // readRequest).
    ReadableByteStreamControllerFillReadRequestFromQueue(aCx, aController,
                                                         readRequest, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-enqueue
void ReadableByteStreamControllerEnqueue(
    JSContext* aCx, ReadableByteStreamController* aController,
    JS::Handle<JSObject*> aChunk, ErrorResult& aRv) {
  aRv.MightThrowJSException();

  // Step 1.
  RefPtr<ReadableStream> stream = aController->Stream();

  // Step 2.
  if (aController->CloseRequested() ||
      stream->State() != ReadableStream::ReaderState::Readable) {
    return;
  }

  // Step 3.
  bool isShared;
  JS::Rooted<JSObject*> buffer(
      aCx, JS_GetArrayBufferViewBuffer(aCx, aChunk, &isShared));
  if (!buffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  // Step 4.
  size_t byteOffset = JS_GetArrayBufferViewByteOffset(aChunk);

  // Step 5.
  size_t byteLength = JS_GetArrayBufferViewByteLength(aChunk);

  // Step 6.
  if (JS::IsDetachedArrayBufferObject(buffer)) {
    aRv.ThrowTypeError("Detatched Array Buffer");
    return;
  }

  // Step 7.
  JS::Rooted<JSObject*> transferredBuffer(aCx,
                                          TransferArrayBuffer(aCx, buffer));
  if (!transferredBuffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  // Step 8.
  if (!aController->PendingPullIntos().isEmpty()) {
    // Step 8.1
    RefPtr<PullIntoDescriptor> firstPendingPullInto =
        aController->PendingPullIntos().getFirst();

    // Step 8.2
    JS::Rooted<JSObject*> pendingBuffer(aCx, firstPendingPullInto->Buffer());
    if (JS::IsDetachedArrayBufferObject(pendingBuffer)) {
      aRv.ThrowTypeError("Pending PullInto has detached buffer");
      return;
    }

    // Step 8.3. Perform !
    // ReadableByteStreamControllerInvalidateBYOBRequest(controller).
    ReadableByteStreamControllerInvalidateBYOBRequest(aController);

    // Step 8.4. Set firstPendingPullInto’s buffer to !
    // TransferArrayBuffer(firstPendingPullInto’s buffer).
    pendingBuffer = TransferArrayBuffer(aCx, pendingBuffer);
    if (!pendingBuffer) {
      aRv.StealExceptionFromJSContext(aCx);
      return;
    }
    firstPendingPullInto->SetBuffer(pendingBuffer);

    // Step 8.5. If firstPendingPullInto’s reader type is "none", perform ?
    // ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(controller,
    // firstPendingPullInto).
    if (firstPendingPullInto->GetReaderType() == ReaderType::None) {
      ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(
          aCx, aController, firstPendingPullInto, aRv);
      if (aRv.Failed()) {
        return;
      }
    }
  }

  // Step 9. If ! ReadableStreamHasDefaultReader(stream) is true,
  if (ReadableStreamHasDefaultReader(stream)) {
    // Step 9.1. Perform !
    // ReadableByteStreamControllerProcessReadRequestsUsingQueue(controller).
    ReadableByteStreamControllerProcessReadRequestsUsingQueue(aCx, aController,
                                                              aRv);
    if (aRv.Failed()) {
      return;
    }

    // Step 9.2. If ! ReadableStreamGetNumReadRequests(stream) is 0,
    if (ReadableStreamGetNumReadRequests(stream) == 0) {
      // Step 9.2.1 Assert: controller.[[pendingPullIntos]] is empty.
      MOZ_ASSERT(aController->PendingPullIntos().isEmpty());

      // Step 9.2.2. Perform !
      // ReadableByteStreamControllerEnqueueChunkToQueue(controller,
      // transferredBuffer, byteOffset, byteLength).
      ReadableByteStreamControllerEnqueueChunkToQueue(
          aController, transferredBuffer, byteOffset, byteLength);

      // Step 9.3. Otherwise,
    } else {
      // Step 9.3.1 Assert: controller.[[queue]] is empty.
      MOZ_ASSERT(aController->Queue().isEmpty());

      // Step 9.3.2. If controller.[[pendingPullIntos]] is not empty,
      if (!aController->PendingPullIntos().isEmpty()) {
        // Step 9.3.2.1. Assert: controller.[[pendingPullIntos]][0]'s reader
        // type is "default".
        MOZ_ASSERT(
            aController->PendingPullIntos().getFirst()->GetReaderType() ==
            ReaderType::Default);

        // Step 9.3.2.2. Perform !
        // ReadableByteStreamControllerShiftPendingPullInto(controller).
        RefPtr<PullIntoDescriptor> pullIntoDescriptor =
            ReadableByteStreamControllerShiftPendingPullInto(aController);
        (void)pullIntoDescriptor;
      }

      // Step 9.3.3. Let transferredView be ! Construct(%Uint8Array%, «
      // transferredBuffer, byteOffset, byteLength »).
      JS::Rooted<JSObject*> transferredView(
          aCx, JS_NewUint8ArrayWithBuffer(aCx, transferredBuffer, byteOffset,
                                          int64_t(byteLength)));
      if (!transferredView) {
        aRv.StealExceptionFromJSContext(aCx);
        return;
      }

      // Step 9.3.4. Perform ! ReadableStreamFulfillReadRequest(stream,
      // transferredView, false).
      JS::Rooted<JS::Value> transferredViewValue(
          aCx, JS::ObjectValue(*transferredView));
      ReadableStreamFulfillReadRequest(aCx, stream, transferredViewValue, false,
                                       aRv);
      if (aRv.Failed()) {
        return;
      }
    }

    // Step 10. Otherwise, if ! ReadableStreamHasBYOBReader(stream) is true,
  } else if (ReadableStreamHasBYOBReader(stream)) {
    // Step 10.1. Perform !
    // ReadableByteStreamControllerEnqueueChunkToQueue(controller,
    // transferredBuffer, byteOffset, byteLength).
    ReadableByteStreamControllerEnqueueChunkToQueue(
        aController, transferredBuffer, byteOffset, byteLength);

    // Step 10.2 Perform !
    // ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(controller).
    ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(
        aCx, aController, aRv);
    if (aRv.Failed()) {
      return;
    }

    // Step 11. Otherwise,
  } else {
    // Step 11.1. Assert: ! IsReadableStreamLocked(stream) is false.
    MOZ_ASSERT(!IsReadableStreamLocked(stream));

    // Step 11.2. Perform !
    // ReadableByteStreamControllerEnqueueChunkToQueue(controller,
    // transferredBuffer, byteOffset, byteLength).
    ReadableByteStreamControllerEnqueueChunkToQueue(
        aController, transferredBuffer, byteOffset, byteLength);
  }

  // Step 12. Perform !
  // ReadableByteStreamControllerCallPullIfNeeded(controller).
  ReadableByteStreamControllerCallPullIfNeeded(aCx, aController, aRv);
}

// https://streams.spec.whatwg.org/#rbs-controller-enqueue
void ReadableByteStreamController::Enqueue(JSContext* aCx,
                                           const ArrayBufferView& aChunk,
                                           ErrorResult& aRv) {
  // Step 1.
  JS::Rooted<JSObject*> chunk(aCx, aChunk.Obj());
  if (JS_GetArrayBufferViewByteLength(chunk) == 0) {
    aRv.ThrowTypeError("Zero Length View");
    return;
  }

  // Step 2.
  bool isShared;
  JS::Rooted<JSObject*> viewedArrayBuffer(
      aCx, JS_GetArrayBufferViewBuffer(aCx, chunk, &isShared));
  if (!viewedArrayBuffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  if (JS::GetArrayBufferByteLength(viewedArrayBuffer) == 0) {
    aRv.ThrowTypeError("Zero Length Buffer");
    return;
  }

  // Step 3.
  if (CloseRequested()) {
    aRv.ThrowTypeError("close requested");
    return;
  }

  // Step 4.
  if (Stream()->State() != ReadableStream::ReaderState::Readable) {
    aRv.ThrowTypeError("Not Readable");
    return;
  }

  // Step 5.
  ReadableByteStreamControllerEnqueue(aCx, this, chunk, aRv);
}

// https://streams.spec.whatwg.org/#rbs-controller-error
void ReadableByteStreamController::Error(JSContext* aCx,
                                         JS::Handle<JS::Value> aErrorValue,
                                         ErrorResult& aRv) {
  // Step 1.
  ReadableByteStreamControllerError(this, aErrorValue, aRv);
}

// https://streams.spec.whatwg.org/#rbs-controller-private-cancel
already_AddRefed<Promise> ReadableByteStreamController::CancelSteps(
    JSContext* aCx, JS::Handle<JS::Value> aReason, ErrorResult& aRv) {
  // Step 1.
  ReadableByteStreamControllerClearPendingPullIntos(this);

  // Step 2.
  ResetQueue(this);

  // Step 3.
  Optional<JS::Handle<JS::Value>> reason(aCx, aReason);
  RefPtr<UnderlyingSourceCancelCallbackHelper> cancelAlgorithm(
      GetCancelAlgorithm());
  RefPtr<Promise> result =
      cancelAlgorithm
          ? cancelAlgorithm->CancelCallback(aCx, reason, aRv)
          : Promise::CreateResolvedWithUndefined(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  // Step 4.
  ReadableByteStreamControllerClearAlgorithms(this);

  // Step 5.
  return result.forget();
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-handle-queue-drain
void ReadableByteStreamControllerHandleQueueDrain(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv) {
  // Step 1.
  MOZ_ASSERT(aController->Stream()->State() ==
             ReadableStream::ReaderState::Readable);

  // Step 2.
  if (aController->QueueTotalSize() == 0 && aController->CloseRequested()) {
    // Step 2.1
    ReadableByteStreamControllerClearAlgorithms(aController);

    // Step 2.2
    RefPtr<ReadableStream> stream = aController->Stream();
    ReadableStreamClose(aCx, stream, aRv);
    return;
  }

  // Step 3.1
  ReadableByteStreamControllerCallPullIfNeeded(aCx, aController, aRv);
}

// https://streams.spec.whatwg.org/#rbs-controller-private-pull
void ReadableByteStreamController::PullSteps(JSContext* aCx,
                                             ReadRequest* aReadRequest,
                                             ErrorResult& aRv) {
  // Step 1.
  ReadableStream* stream = Stream();

  // Step 2.
  MOZ_ASSERT(ReadableStreamHasDefaultReader(stream));

  // Step 3.
  if (QueueTotalSize() > 0) {
    // Step 3.1. Assert: ! ReadableStreamGetNumReadRequests ( stream ) is 0.
    MOZ_ASSERT(ReadableStreamGetNumReadRequests(stream) == 0);

    // Step 3.2. Perform !
    // ReadableByteStreamControllerFillReadRequestFromQueue(this, readRequest).
    ReadableByteStreamControllerFillReadRequestFromQueue(aCx, this,
                                                         aReadRequest, aRv);

    // Step 3.3. Return.
    return;
  }

  // Step 4.
  Maybe<uint64_t> autoAllocateChunkSize = AutoAllocateChunkSize();

  // Step 5.
  if (autoAllocateChunkSize) {
    // Step 5.1
    aRv.MightThrowJSException();
    JS::Rooted<JSObject*> buffer(
        aCx, JS::NewArrayBuffer(aCx, *autoAllocateChunkSize));
    // Step 5.2
    if (!buffer) {
      // Step 5.2.1
      JS::RootedValue bufferError(aCx);
      if (!JS_GetPendingException(aCx, &bufferError)) {
        // Uncatchable exception; we should mark aRv and return.
        aRv.StealExceptionFromJSContext(aCx);
        return;
      }

      // It's not expliclitly stated, but I assume the intention here is that
      // we perform a normal completion here.
      JS_ClearPendingException(aCx);

      aReadRequest->ErrorSteps(aCx, bufferError, aRv);

      // Step 5.2.2.
      return;
    }

    // Step 5.3
    RefPtr<PullIntoDescriptor> pullIntoDescriptor = new PullIntoDescriptor(
        buffer, *autoAllocateChunkSize, 0, *autoAllocateChunkSize, 0, 1,
        PullIntoDescriptor::Constructor::Uint8, ReaderType::Default);

    //  Step 5.4
    PendingPullIntos().insertBack(pullIntoDescriptor);
  }

  // Step 6.
  ReadableStreamAddReadRequest(stream, aReadRequest);

  // Step 7.
  ReadableByteStreamControllerCallPullIfNeeded(aCx, this, aRv);
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontroller-releasesteps
void ReadableByteStreamController::ReleaseSteps() {
  // Step 1. If this.[[pendingPullIntos]] is not empty,
  if (!PendingPullIntos().isEmpty()) {
    // Step 1.1. Let firstPendingPullInto be this.[[pendingPullIntos]][0].
    RefPtr<PullIntoDescriptor> firstPendingPullInto =
        PendingPullIntos().popFirst();

    // Step 1.2. Set firstPendingPullInto’s reader type to "none".
    firstPendingPullInto->SetReaderType(ReaderType::None);

    // Step 1.3. Set this.[[pendingPullIntos]] to the list «
    // firstPendingPullInto ».
    PendingPullIntos().clear();
    PendingPullIntos().insertBack(firstPendingPullInto);
  }
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-shift-pending-pull-into
already_AddRefed<PullIntoDescriptor>
ReadableByteStreamControllerShiftPendingPullInto(
    ReadableByteStreamController* aController) {
  // Step 1.
  MOZ_ASSERT(!aController->GetByobRequest());

  // Step 2 + 3
  RefPtr<PullIntoDescriptor> descriptor =
      aController->PendingPullIntos().popFirst();

  // Step 4.
  return descriptor.forget();
}

JSObject* ConstructFromPullIntoConstructor(
    JSContext* aCx, PullIntoDescriptor::Constructor constructor,
    JS::HandleObject buffer, size_t byteOffset, size_t length) {
  switch (constructor) {
    case PullIntoDescriptor::Constructor::DataView:
      return JS_NewDataView(aCx, buffer, byteOffset, length);
      break;

#define CONSTRUCT_TYPED_ARRAY_TYPE(ExternalT, NativeT, Name)      \
  case PullIntoDescriptor::Constructor::Name:                     \
    return JS_New##Name##ArrayWithBuffer(aCx, buffer, byteOffset, \
                                         int64_t(length));        \
    break;

      JS_FOR_EACH_TYPED_ARRAY(CONSTRUCT_TYPED_ARRAY_TYPE)

#undef CONSTRUCT_TYPED_ARRAY_TYPE
  }
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-convert-pull-into-descriptor
JSObject* ReadableByteStreamControllerConvertPullIntoDescriptor(
    JSContext* aCx, PullIntoDescriptor* pullIntoDescriptor, ErrorResult& aRv) {
  // Step 1. Let bytesFilled be pullIntoDescriptor’s bytes filled.
  uint64_t bytesFilled = pullIntoDescriptor->BytesFilled();

  // Step 2. Let elementSize be pullIntoDescriptor’s element size.
  uint64_t elementSize = pullIntoDescriptor->ElementSize();

  // Step 3. Assert: bytesFilled ≤ pullIntoDescriptor’s byte length.
  MOZ_ASSERT(bytesFilled <= pullIntoDescriptor->ByteLength());

  // Step 4. Assert: bytesFilled mod elementSize is 0.
  MOZ_ASSERT(bytesFilled % elementSize == 0);

  // Step 5. Let buffer be ! TransferArrayBuffer(pullIntoDescriptor’s buffer).
  aRv.MightThrowJSException();
  JS::Rooted<JSObject*> srcBuffer(aCx, pullIntoDescriptor->Buffer());
  JS::Rooted<JSObject*> buffer(aCx, TransferArrayBuffer(aCx, srcBuffer));
  if (!buffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return nullptr;
  }

  // Step 6. Return ! Construct(pullIntoDescriptor’s view constructor,
  //  « buffer, pullIntoDescriptor’s byte offset, bytesFilled ÷ elementSize »).
  JS::Rooted<JSObject*> res(
      aCx, ConstructFromPullIntoConstructor(
               aCx, pullIntoDescriptor->ViewConstructor(), buffer,
               pullIntoDescriptor->ByteOffset(), bytesFilled / elementSize));
  if (!res) {
    aRv.StealExceptionFromJSContext(aCx);
    return nullptr;
  }
  return res;
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-in-closed-state
MOZ_CAN_RUN_SCRIPT
static void ReadableByteStreamControllerRespondInClosedState(
    JSContext* aCx, ReadableByteStreamController* aController,
    RefPtr<PullIntoDescriptor>& aFirstDescriptor, ErrorResult& aRv) {
  // Step 1. Assert: firstDescriptor ’s bytes filled is 0.
  MOZ_ASSERT(aFirstDescriptor->BytesFilled() == 0);

  // Step 2. If firstDescriptor’s reader type is "none",
  // perform ! ReadableByteStreamControllerShiftPendingPullInto(controller).
  if (aFirstDescriptor->GetReaderType() == ReaderType::None) {
    RefPtr<PullIntoDescriptor> discarded =
        ReadableByteStreamControllerShiftPendingPullInto(aController);
    (void)discarded;
  }

  // Step 3. Let stream be controller.[[stream]].
  RefPtr<ReadableStream> stream = aController->Stream();

  // Step 4. If ! ReadableStreamHasBYOBReader(stream) is true,
  if (ReadableStreamHasBYOBReader(stream)) {
    // Step 4.1. While ! ReadableStreamGetNumReadIntoRequests(stream) > 0,
    while (ReadableStreamGetNumReadIntoRequests(stream) > 0) {
      // Step 4.1.1. Let pullIntoDescriptor be !
      // ReadableByteStreamControllerShiftPendingPullInto(controller).
      RefPtr<PullIntoDescriptor> pullIntoDescriptor =
          ReadableByteStreamControllerShiftPendingPullInto(aController);

      // Step 4.1.2. Perform !
      // ReadableByteStreamControllerCommitPullIntoDescriptor(stream,
      // pullIntoDescriptor).
      ReadableByteStreamControllerCommitPullIntoDescriptor(
          aCx, stream, pullIntoDescriptor, aRv);
    }
  }
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-fill-head-pull-into-descriptor
void ReadableByteStreamControllerFillHeadPullIntoDescriptor(
    ReadableByteStreamController* aController, size_t aSize,
    PullIntoDescriptor* aPullIntoDescriptor) {
  // Step 1. Assert: either controller.[[pendingPullIntos]] is empty, or
  // controller.[[pendingPullIntos]][0] is pullIntoDescriptor.
  MOZ_ASSERT(aController->PendingPullIntos().isEmpty() ||
             aController->PendingPullIntos().getFirst() == aPullIntoDescriptor);

  // Step 2. Assert: controller.[[byobRequest]] is null.
  MOZ_ASSERT(!aController->GetByobRequest());

  // Step 3. Set pullIntoDescriptor’s bytes filled to bytes filled + size.
  aPullIntoDescriptor->SetBytesFilled(aPullIntoDescriptor->BytesFilled() +
                                      aSize);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-in-readable-state
MOZ_CAN_RUN_SCRIPT
static void ReadableByteStreamControllerRespondInReadableState(
    JSContext* aCx, ReadableByteStreamController* aController,
    uint64_t aBytesWritten, PullIntoDescriptor* aPullIntoDescriptor,
    ErrorResult& aRv) {
  // Step 1. Assert: pullIntoDescriptor’s bytes filled + bytesWritten ≤
  // pullIntoDescriptor’s byte length.
  MOZ_ASSERT(aPullIntoDescriptor->BytesFilled() + aBytesWritten <=
             aPullIntoDescriptor->ByteLength());

  // Step 2. Perform
  // !ReadableByteStreamControllerFillHeadPullIntoDescriptor(controller,
  // bytesWritten, pullIntoDescriptor).
  ReadableByteStreamControllerFillHeadPullIntoDescriptor(
      aController, aBytesWritten, aPullIntoDescriptor);

  // Step 3. If pullIntoDescriptor’s reader type is "none",
  if (aPullIntoDescriptor->GetReaderType() == ReaderType::None) {
    // Step 3.1. Perform ?
    // ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(controller,
    // pullIntoDescriptor).
    ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(
        aCx, aController, aPullIntoDescriptor, aRv);
    if (aRv.Failed()) {
      return;
    }

    // Step 3.2. Perform !
    // ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(controller).
    ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(
        aCx, aController, aRv);

    // Step 3.3. Return.
    return;
  }

  // Step 4. If pullIntoDescriptor’s bytes filled < pullIntoDescriptor’s element
  // size, return.
  if (aPullIntoDescriptor->BytesFilled() < aPullIntoDescriptor->ElementSize()) {
    return;
  }

  // Step 5. Perform
  // !ReadableByteStreamControllerShiftPendingPullInto(controller).
  RefPtr<PullIntoDescriptor> pullIntoDescriptor =
      ReadableByteStreamControllerShiftPendingPullInto(aController);
  (void)pullIntoDescriptor;

  // Step 6. Let remainderSize be pullIntoDescriptor’s bytes filled mod
  // pullIntoDescriptor’s element size.
  size_t remainderSize =
      aPullIntoDescriptor->BytesFilled() % aPullIntoDescriptor->ElementSize();

  // Step 7. If remainderSize > 0,
  if (remainderSize > 0) {
    // Step 7.1. Let end be pullIntoDescriptor’s byte offset +
    // pullIntoDescriptor’s bytes filled.
    size_t end =
        aPullIntoDescriptor->ByteOffset() + aPullIntoDescriptor->BytesFilled();

    // Step 7.2. Perform ?
    // ReadableByteStreamControllerEnqueueClonedChunkToQueue(controller,
    // pullIntoDescriptor’s buffer, end − remainderSize, remainderSize).
    JS::Rooted<JSObject*> pullIntoBuffer(aCx, aPullIntoDescriptor->Buffer());
    ReadableByteStreamControllerEnqueueClonedChunkToQueue(
        aCx, aController, pullIntoBuffer, end - remainderSize, remainderSize,
        aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 8. Set pullIntoDescriptor’s bytes filled to pullIntoDescriptor’s bytes
  // filled − remainderSize.
  aPullIntoDescriptor->SetBytesFilled(aPullIntoDescriptor->BytesFilled() -
                                      remainderSize);

  // Step 9. Perform
  // !ReadableByteStreamControllerCommitPullIntoDescriptor(controller.[[stream]],
  // pullIntoDescriptor).
  RefPtr<ReadableStream> stream(aController->Stream());
  ReadableByteStreamControllerCommitPullIntoDescriptor(
      aCx, stream, aPullIntoDescriptor, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 10. Perform
  // !ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(controller).
  ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(
      aCx, aController, aRv);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-internal
void ReadableByteStreamControllerRespondInternal(
    JSContext* aCx, ReadableByteStreamController* aController,
    uint64_t aBytesWritten, ErrorResult& aRv) {
  // Step 1.
  RefPtr<PullIntoDescriptor> firstDescriptor =
      aController->PendingPullIntos().getFirst();

  // Step 2.
  JS::Rooted<JSObject*> buffer(aCx, firstDescriptor->Buffer());
#ifdef DEBUG
  bool canTransferBuffer = CanTransferArrayBuffer(aCx, buffer, aRv);
  MOZ_ASSERT(!aRv.Failed());
  MOZ_ASSERT(canTransferBuffer);
#endif

  // Step 3.
  ReadableByteStreamControllerInvalidateBYOBRequest(aController);

  // Step 4.
  auto state = aController->Stream()->State();

  // Step 5.
  if (state == ReadableStream::ReaderState::Closed) {
    // Step 5.1
    MOZ_ASSERT(aBytesWritten == 0);

    // Step 5.2
    ReadableByteStreamControllerRespondInClosedState(aCx, aController,
                                                     firstDescriptor, aRv);
    if (aRv.Failed()) {
      return;
    }
  } else {
    // Step 6.1
    MOZ_ASSERT(state == ReadableStream::ReaderState::Readable);

    // Step 6.2.
    MOZ_ASSERT(aBytesWritten > 0);

    // Step 6.3
    ReadableByteStreamControllerRespondInReadableState(
        aCx, aController, aBytesWritten, firstDescriptor, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  // Step 7.
  ReadableByteStreamControllerCallPullIfNeeded(aCx, aController, aRv);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond
void ReadableByteStreamControllerRespond(
    JSContext* aCx, ReadableByteStreamController* aController,
    uint64_t aBytesWritten, ErrorResult& aRv) {
  // Step 1.
  MOZ_ASSERT(!aController->PendingPullIntos().isEmpty());

  // Step 2.
  PullIntoDescriptor* firstDescriptor =
      aController->PendingPullIntos().getFirst();

  // Step 3.
  auto state = aController->Stream()->State();

  // Step 4.
  if (state == ReadableStream::ReaderState::Closed) {
    // Step 4.1
    if (aBytesWritten != 0) {
      aRv.ThrowTypeError("bytesWritten not zero on closed stream");
      return;
    }
  } else {
    // Step 5.1
    MOZ_ASSERT(state == ReadableStream::ReaderState::Readable);

    // Step 5.2
    if (aBytesWritten == 0) {
      aRv.ThrowTypeError("bytesWritten 0");
      return;
    }

    // Step 5.3
    if (firstDescriptor->BytesFilled() + aBytesWritten >
        firstDescriptor->ByteLength()) {
      aRv.ThrowRangeError("bytesFilled + bytesWritten > byteLength");
      return;
    }
  }

  // Step 6.
  aRv.MightThrowJSException();
  JS::Rooted<JSObject*> buffer(aCx, firstDescriptor->Buffer());
  JS::Rooted<JSObject*> transferredBuffer(aCx,
                                          TransferArrayBuffer(aCx, buffer));
  if (!transferredBuffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  firstDescriptor->SetBuffer(transferredBuffer);

  // Step 7.
  ReadableByteStreamControllerRespondInternal(aCx, aController, aBytesWritten,
                                              aRv);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-with-new-view
void ReadableByteStreamControllerRespondWithNewView(
    JSContext* aCx, ReadableByteStreamController* aController,
    JS::Handle<JSObject*> aView, ErrorResult& aRv) {
  aRv.MightThrowJSException();

  // Step 1.
  MOZ_ASSERT(!aController->PendingPullIntos().isEmpty());

  // Step 2.
  bool isSharedMemory;
  JS::Rooted<JSObject*> viewedArrayBuffer(
      aCx, JS_GetArrayBufferViewBuffer(aCx, aView, &isSharedMemory));
  if (!viewedArrayBuffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  MOZ_ASSERT(!JS::IsDetachedArrayBufferObject(viewedArrayBuffer));

  // Step 3.
  RefPtr<PullIntoDescriptor> firstDescriptor =
      aController->PendingPullIntos().getFirst();

  // Step 4.
  ReadableStream::ReaderState state = aController->Stream()->State();

  // Step 5.
  if (state == ReadableStream::ReaderState::Closed) {
    // Step 5.1
    if (JS_GetArrayBufferViewByteLength(aView) != 0) {
      aRv.ThrowTypeError("View has non-zero length in closed stream");
      return;
    }
  } else {
    // Step 6.1
    MOZ_ASSERT(state == ReadableStream::ReaderState::Readable);

    // Step 6.2
    if (JS_GetArrayBufferViewByteLength(aView) == 0) {
      aRv.ThrowTypeError("View has zero length in readable stream");
      return;
    }
  }

  // Step 7.
  if (firstDescriptor->ByteOffset() + firstDescriptor->BytesFilled() !=
      JS_GetArrayBufferViewByteOffset(aView)) {
    aRv.ThrowRangeError("Invalid Offset");
    return;
  }

  // Step 8.
  if (firstDescriptor->BufferByteLength() !=
      JS::GetArrayBufferByteLength(viewedArrayBuffer)) {
    aRv.ThrowRangeError("Mismatched buffer byte lengths");
    return;
  }

  // Step 9.
  if (firstDescriptor->BytesFilled() + JS_GetArrayBufferViewByteLength(aView) >
      firstDescriptor->ByteLength()) {
    aRv.ThrowRangeError("Too many bytes");
    return;
  }

  // Step 10. Let viewByteLength be view.[[ByteLength]].
  size_t viewByteLength = JS_GetArrayBufferViewByteLength(aView);

  // Step 11. Set firstDescriptor’s buffer to ?
  // TransferArrayBuffer(view.[[ViewedArrayBuffer]]).
  JS::Rooted<JSObject*> transferedBuffer(
      aCx, TransferArrayBuffer(aCx, viewedArrayBuffer));
  if (!transferedBuffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  firstDescriptor->SetBuffer(transferedBuffer);

  // Step 12. Perform ? ReadableByteStreamControllerRespondInternal(controller,
  // viewByteLength).
  ReadableByteStreamControllerRespondInternal(aCx, aController, viewByteLength,
                                              aRv);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-fill-pull-into-descriptor-from-queue
bool ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(
    JSContext* aCx, ReadableByteStreamController* aController,
    PullIntoDescriptor* aPullIntoDescriptor, ErrorResult& aRv) {
  // Step 1. Let elementSize be pullIntoDescriptor.[[elementSize]].
  size_t elementSize = aPullIntoDescriptor->ElementSize();

  // Step 2. Let currentAlignedBytes be pullIntoDescriptor’s bytes filled −
  // (pullIntoDescriptor’s bytes filled mod elementSize).
  size_t currentAlignedBytes =
      aPullIntoDescriptor->BytesFilled() -
      (aPullIntoDescriptor->BytesFilled() % elementSize);

  // Step 3. Let maxBytesToCopy be min(controller.[[queueTotalSize]],
  // pullIntoDescriptor’s byte length − pullIntoDescriptor’s bytes filled).
  size_t maxBytesToCopy =
      std::min(static_cast<size_t>(aController->QueueTotalSize()),
               static_cast<size_t>((aPullIntoDescriptor->ByteLength() -
                                    aPullIntoDescriptor->BytesFilled())));

  // Step 4. Let maxBytesFilled be pullIntoDescriptor’s bytes filled +
  // maxBytesToCopy.
  size_t maxBytesFilled = aPullIntoDescriptor->BytesFilled() + maxBytesToCopy;

  // Step 5. Let maxAlignedBytes be maxBytesFilled − (maxBytesFilled mod
  // elementSize).
  size_t maxAlignedBytes = maxBytesFilled - (maxBytesFilled % elementSize);

  // Step 6. Let totalBytesToCopyRemaining be maxBytesToCopy.
  size_t totalBytesToCopyRemaining = maxBytesToCopy;

  // Step 7. Let ready be false.
  bool ready = false;

  // Step 8. If maxAlignedBytes > currentAlignedBytes,
  if (maxAlignedBytes > currentAlignedBytes) {
    // Step 8.1. Set totalBytesToCopyRemaining to maxAlignedBytes −
    // pullIntoDescriptor’s bytes filled.
    totalBytesToCopyRemaining =
        maxAlignedBytes - aPullIntoDescriptor->BytesFilled();
    // Step 8.2. Set ready to true.
    ready = true;
  }

  // Step 9. Let queue be controller.[[queue]].
  LinkedList<RefPtr<ReadableByteStreamQueueEntry>>& queue =
      aController->Queue();

  // Step 10. While totalBytesToCopyRemaining > 0,
  while (totalBytesToCopyRemaining > 0) {
    // Step 10.1  Let headOfQueue be queue[0].
    ReadableByteStreamQueueEntry* headOfQueue = queue.getFirst();

    // Step 10.2. Let bytesToCopy be min(totalBytesToCopyRemaining,
    // headOfQueue’s byte length).
    size_t bytesToCopy =
        std::min(totalBytesToCopyRemaining, headOfQueue->ByteLength());

    // Step 10.3. Let destStart be pullIntoDescriptor’s byte offset +
    // pullIntoDescriptor’s bytes filled.
    size_t destStart =
        aPullIntoDescriptor->ByteOffset() + aPullIntoDescriptor->BytesFilled();

    // Step 10.4. Perform !CopyDataBlockBytes(pullIntoDescriptor’s
    //            buffer.[[ArrayBufferData]], destStart, headOfQueue’s
    //            buffer.[[ArrayBufferData]], headOfQueue’s byte offset,
    //            bytesToCopy).
    JS::RootedObject descriptorBuffer(aCx, aPullIntoDescriptor->Buffer());
    JS::RootedObject queueBuffer(aCx, headOfQueue->Buffer());
    if (!JS::ArrayBufferCopyData(aCx, descriptorBuffer, destStart, queueBuffer,
                                 headOfQueue->ByteOffset(), bytesToCopy)) {
      aRv.StealExceptionFromJSContext(aCx);
      return false;
    }

    // Step 10.5. If headOfQueue’s byte length is bytesToCopy,
    if (headOfQueue->ByteLength() == bytesToCopy) {
      // Step 10.5.1. Remove queue[0].
      queue.popFirst();
    } else {
      // Step 10.6.  Otherwise,

      // Step 10.6.1 Set headOfQueue’s byte offset to
      // headOfQueue’s byte offset +  bytesToCopy.
      headOfQueue->SetByteOffset(headOfQueue->ByteOffset() + bytesToCopy);
      // Step 10.6.2  Set headOfQueue’s byte length to
      // headOfQueue’s byte length − bytesToCopy.
      headOfQueue->SetByteLength(headOfQueue->ByteLength() - bytesToCopy);
    }

    // Step 10.7. Set controller.[[queueTotalSize]] to
    // controller.[[queueTotalSize]] −  bytesToCopy.
    aController->SetQueueTotalSize(aController->QueueTotalSize() -
                                   (double)bytesToCopy);

    // Step 10.8, Perform
    // !ReadableByteStreamControllerFillHeadPullIntoDescriptor(controller,
    //     bytesToCopy, pullIntoDescriptor).
    ReadableByteStreamControllerFillHeadPullIntoDescriptor(
        aController, bytesToCopy, aPullIntoDescriptor);

    // Step 10.9. Set totalBytesToCopyRemaining to totalBytesToCopyRemaining −
    //     bytesToCopy.
    totalBytesToCopyRemaining = totalBytesToCopyRemaining - bytesToCopy;
  }

  // Step 11. If ready is false,
  if (!ready) {
    // Step 11.1. Assert: controller.[[queueTotalSize]] is 0.
    MOZ_ASSERT(aController->QueueTotalSize() == 0);

    // Step 11.2. Assert: pullIntoDescriptor’s bytes filled > 0.
    MOZ_ASSERT(aPullIntoDescriptor->BytesFilled() > 0);

    // Step 11.3. Assert: pullIntoDescriptor’s bytes filled <
    // pullIntoDescriptor’s
    //     element size.
    MOZ_ASSERT(aPullIntoDescriptor->BytesFilled() <
               aPullIntoDescriptor->ElementSize());
  }

  // Step 12. Return ready.
  return ready;
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-pull-into
void ReadableByteStreamControllerPullInto(
    JSContext* aCx, ReadableByteStreamController* aController,
    JS::HandleObject aView, ReadIntoRequest* aReadIntoRequest,
    ErrorResult& aRv) {
  aRv.MightThrowJSException();

  // Step 1. Let stream be controller.[[stream]].
  ReadableStream* stream = aController->Stream();

  // Step 2. Let elementSize be 1.
  size_t elementSize = 1;

  // Step 3. Let ctor be %DataView%.
  PullIntoDescriptor::Constructor ctor =
      PullIntoDescriptor::Constructor::DataView;

  // Step 4. If view has a [[TypedArrayName]] internal slot (i.e., it is not a
  // DataView),
  if (JS_IsTypedArrayObject(aView)) {
    // Step 4.1. Set elementSize to the element size specified in the typed
    // array constructors table for view.[[TypedArrayName]].
    JS::Scalar::Type type = JS_GetArrayBufferViewType(aView);
    elementSize = JS::Scalar::byteSize(type);

    // Step 4.2 Set ctor to the constructor specified in the typed array
    // constructors table for view.[[TypedArrayName]].
    ctor = PullIntoDescriptor::constructorFromScalar(type);
  }

  // Step 5. Let byteOffset be view.[[ByteOffset]].
  size_t byteOffset = JS_GetArrayBufferViewByteOffset(aView);

  // Step 6. Let byteLength be view.[[ByteLength]].
  size_t byteLength = JS_GetArrayBufferViewByteLength(aView);

  // Step 7. Let bufferResult be
  // TransferArrayBuffer(view.[[ViewedArrayBuffer]]).
  bool isShared;
  JS::RootedObject viewedArrayBuffer(
      aCx, JS_GetArrayBufferViewBuffer(aCx, aView, &isShared));
  if (!viewedArrayBuffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  JS::RootedObject bufferResult(aCx,
                                TransferArrayBuffer(aCx, viewedArrayBuffer));

  // Step 8. If bufferResult is an abrupt completion,
  if (!bufferResult) {
    JS::RootedValue pendingException(aCx);
    if (!JS_GetPendingException(aCx, &pendingException)) {
      // This means an un-catchable exception. Use StealExceptionFromJSContext
      // to setup aRv properly.
      aRv.StealExceptionFromJSContext(aCx);
      return;
    }

    // It's not expliclitly stated, but I assume the intention here is that
    // we perform a normal completion here; we also need to clear the
    // exception state anyhow to succesfully run ErrorSteps.
    JS_ClearPendingException(aCx);

    //     Step 8.1. Perform readIntoRequest’s error steps, given
    //     bufferResult.[[Value]].
    aReadIntoRequest->ErrorSteps(aCx, pendingException, aRv);

    //     Step 8.2. Return.
    return;
  }

  // Step 9. Let buffer be bufferResult.[[Value]].
  JS::RootedObject buffer(aCx, bufferResult);

  // Step 10. Let pullIntoDescriptor be a new pull-into descriptor with
  //  buffer: buffer,
  //  buffer byte length:  buffer.[[ArrayBufferByteLength]],
  //  byte offset: byteOffset,
  //  byte length: byteLength,
  //  bytes filled: 0,
  //  element size: elementSize,
  //  view constructor: ctor,
  //  and reader type: "byob".
  RefPtr<PullIntoDescriptor> pullIntoDescriptor = new PullIntoDescriptor(
      buffer, JS::GetArrayBufferByteLength(buffer), byteOffset, byteLength, 0,
      elementSize, ctor, ReaderType::BYOB);

  // Step 11. If controller.[[pendingPullIntos]] is not empty,
  if (!aController->PendingPullIntos().isEmpty()) {
    // Step 11.1. Append pullIntoDescriptor to controller.[[pendingPullIntos]].
    aController->PendingPullIntos().insertBack(pullIntoDescriptor);

    // Step 11.2. Perform !ReadableStreamAddReadIntoRequest(stream,
    // readIntoRequest).
    ReadableStreamAddReadIntoRequest(stream, aReadIntoRequest);

    // Step 11.3. Return.
    return;
  }

  // Step 12. If stream.[[state]] is "closed",
  if (stream->State() == ReadableStream::ReaderState::Closed) {
    // Step 12.1. Let emptyView be !Construct(ctor, « pullIntoDescriptor’s
    //            buffer, pullIntoDescriptor’s byte offset, 0 »).
    JS::RootedObject pullIntoBuffer(aCx, pullIntoDescriptor->Buffer());
    JS::RootedObject emptyView(aCx, ConstructFromPullIntoConstructor(
                                        aCx, ctor, pullIntoBuffer,
                                        pullIntoDescriptor->ByteOffset(), 0));
    if (!emptyView) {
      aRv.StealExceptionFromJSContext(aCx);
      return;
    }

    // Step 12.2. Perform readIntoRequest’s close steps, given emptyView.
    JS::RootedValue emptyViewValue(aCx, JS::ObjectValue(*emptyView));
    aReadIntoRequest->CloseSteps(aCx, emptyViewValue, aRv);

    // Step 12.3. Return.
    return;
  }

  // Step 13,. If controller.[[queueTotalSize]] > 0,
  if (aController->QueueTotalSize() > 0) {
    // Step 13.1 If
    // !ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(controller,
    //     pullIntoDescriptor) is true,
    bool ready = ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(
        aCx, aController, pullIntoDescriptor, aRv);
    if (aRv.Failed()) {
      return;
    }
    if (ready) {
      // Step 13.1.1  Let filledView be
      //         !ReadableByteStreamControllerConvertPullIntoDescriptor(pullIntoDescriptor).
      JS::Rooted<JSObject*> filledView(
          aCx, ReadableByteStreamControllerConvertPullIntoDescriptor(
                   aCx, pullIntoDescriptor, aRv));
      if (aRv.Failed()) {
        return;
      }
      //  Step 13.1.2. Perform
      //         !ReadableByteStreamControllerHandleQueueDrain(controller).
      ReadableByteStreamControllerHandleQueueDrain(aCx, aController, aRv);
      if (aRv.Failed()) {
        return;
      }
      // Step 13.1.3.  Perform readIntoRequest’s chunk steps, given filledView.
      JS::RootedValue filledViewValue(aCx, JS::ObjectValue(*filledView));
      aReadIntoRequest->ChunkSteps(aCx, filledViewValue, aRv);
      // Step 13.1.4.   Return.
      return;
    }

    // Step 13.2  If controller.[[closeRequested]] is true,
    if (aController->CloseRequested()) {
      // Step 13.2.1.   Let e be a TypeError exception.
      ErrorResult typeError;
      typeError.ThrowTypeError("Close Requested True during Pull Into");

      JS::RootedValue e(aCx);
      MOZ_RELEASE_ASSERT(ToJSValue(aCx, std::move(typeError), &e));

      // Step 13.2.2. Perform !ReadableByteStreamControllerError(controller, e).
      ReadableByteStreamControllerError(aController, e, aRv);
      if (aRv.Failed()) {
        return;
      }

      // Step 13.2.3. Perform readIntoRequest’s error steps, given e.
      aReadIntoRequest->ErrorSteps(aCx, e, aRv);

      // Step 13.2.4.  Return.
      return;
    }
  }

  // Step 14. Append pullIntoDescriptor to controller.[[pendingPullIntos]].
  aController->PendingPullIntos().insertBack(pullIntoDescriptor);

  // Step 15. Perform !ReadableStreamAddReadIntoRequest(stream,
  // readIntoRequest).
  ReadableStreamAddReadIntoRequest(stream, aReadIntoRequest);

  // Step 16, Perform
  // !ReadableByteStreamControllerCallPullIfNeeded(controller).
  ReadableByteStreamControllerCallPullIfNeeded(aCx, aController, aRv);
}

class ByteStreamStartPromiseNativeHandler final : public PromiseNativeHandler {
  ~ByteStreamStartPromiseNativeHandler() = default;

  RefPtr<ReadableByteStreamController> mController;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ByteStreamStartPromiseNativeHandler)

  explicit ByteStreamStartPromiseNativeHandler(
      ReadableByteStreamController* aController)
      : PromiseNativeHandler(), mController(aController) {}

  MOZ_CAN_RUN_SCRIPT
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    MOZ_ASSERT(mController);

    // https://streams.spec.whatwg.org/#set-up-readable-byte-stream-controller
    //
    // Step 16.1
    mController->SetStarted(true);

    // Step 16.2
    mController->SetPulling(false);

    // Step 16.3
    mController->SetPullAgain(false);

    // Step 16.4:

    RefPtr<ReadableByteStreamController> stackController = mController;
    ReadableByteStreamControllerCallPullIfNeeded(aCx, stackController, aRv);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#set-up-readable-byte-stream-controller
    // Step 17.1
    ReadableByteStreamControllerError(mController, aValue, aRv);
  }
};

// Cycle collection methods for promise handler
NS_IMPL_CYCLE_COLLECTION(ByteStreamStartPromiseNativeHandler, mController)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ByteStreamStartPromiseNativeHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ByteStreamStartPromiseNativeHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ByteStreamStartPromiseNativeHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#set-up-readable-byte-stream-controller
void SetUpReadableByteStreamController(
    JSContext* aCx, ReadableStream* aStream,
    ReadableByteStreamController* aController,
    UnderlyingSourceStartCallbackHelper* aStartAlgorithm,
    UnderlyingSourcePullCallbackHelper* aPullAlgorithm,
    UnderlyingSourceCancelCallbackHelper* aCancelAlgorithm,
    UnderlyingSourceErrorCallbackHelper* aErrorAlgorithm, double aHighWaterMark,
    Maybe<uint64_t> aAutoAllocateChunkSize, ErrorResult& aRv) {
  // Step 1. Assert: stream.[[controller]] is undefined.
  MOZ_ASSERT(!aStream->Controller());

  // Step 2. If autoAllocateChunkSize is not undefined,
  if (aAutoAllocateChunkSize) {
    // Step 2.1. Assert: ! IsInteger(autoAllocateChunkSize) is true. Implicit
    // Step 2.2. Assert: autoAllocateChunkSize is positive.
    MOZ_ASSERT(*aAutoAllocateChunkSize >= 0);
  }

  // Step 3. Set controller.[[stream]] to stream.
  aController->SetStream(aStream);

  // Step 4. Set controller.[[pullAgain]] and controller.[[pulling]] to false.
  aController->SetPullAgain(false);
  aController->SetPulling(false);

  // Step 5. Set controller.[[byobRequest]] to null.
  aController->SetByobRequest(nullptr);

  // Step 6. Perform !ResetQueue(controller).
  ResetQueue(aController);

  // Step 7. Set controller.[[closeRequested]] and controller.[[started]] to
  // false.
  aController->SetCloseRequested(false);
  aController->SetStarted(false);

  // Step 8. Set controller.[[strategyHWM]] to highWaterMark.
  aController->SetStrategyHWM(aHighWaterMark);

  // Step 9. Set controller.[[pullAlgorithm]] to pullAlgorithm.
  aController->SetPullAlgorithm(aPullAlgorithm);

  // Step 10. Set controller.[[cancelAlgorithm]] to cancelAlgorithm.
  aController->SetCancelAlgorithm(aCancelAlgorithm);

  // Not Specified.
  aStream->SetErrorAlgorithm(aErrorAlgorithm);

  // Step 11. Set controller.[[autoAllocateChunkSize]] to autoAllocateChunkSize.
  aController->SetAutoAllocateChunkSize(aAutoAllocateChunkSize);

  // Step 12. Set controller.[[pendingPullIntos]] to a new empty list.
  aController->PendingPullIntos().clear();

  // Step 13. Set stream.[[controller]] to controller.
  aStream->SetController(aController);

  // Step 14. Let startResult be the result of performing startAlgorithm.
  // Default algorithm returns undefined.
  JS::RootedValue startResult(aCx, JS::UndefinedValue());
  if (aStartAlgorithm) {
    // Strong Refs:
    RefPtr<UnderlyingSourceStartCallbackHelper> startAlgorithm(aStartAlgorithm);
    RefPtr<ReadableStreamController> controller(aController);

    startAlgorithm->StartCallback(aCx, *controller, &startResult, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Let startPromise be a promise resolved with startResult.
  RefPtr<Promise> startPromise = Promise::Create(GetIncumbentGlobal(), aRv);
  if (aRv.Failed()) {
    return;
  }
  startPromise->MaybeResolve(startResult);

  // Step 16+17
  startPromise->AppendNativeHandler(
      new ByteStreamStartPromiseNativeHandler(aController));
}

// https://streams.spec.whatwg.org/#set-up-readable-byte-stream-controller-from-underlying-source
void SetUpReadableByteStreamControllerFromUnderlyingSource(
    JSContext* aCx, ReadableStream* aStream, JS::HandleObject aUnderlyingSource,
    UnderlyingSource& aUnderlyingSourceDict, double aHighWaterMark,
    ErrorResult& aRv) {
  // Step 1. Let controller be a new ReadableByteStreamController.
  RefPtr<ReadableByteStreamController> controller =
      new ReadableByteStreamController(aStream->GetParentObject());

  // Step 2. Let startAlgorithm be an algorithm that returns undefined.
  RefPtr<UnderlyingSourceStartCallbackHelper> startAlgorithm;

  // Step 3. Let pullAlgorithm be an algorithm that returns a promise resolved
  // with undefined.
  RefPtr<UnderlyingSourcePullCallbackHelper> pullAlgorithm;

  // Step 4. Let cancelAlgorithm be an algorithm that returns a promise resolved
  // with undefined.
  RefPtr<UnderlyingSourceCancelCallbackHelper> cancelAlgorithm;

  // Step 5. If underlyingSourceDict["start"] exists, then set startAlgorithm to
  // an algorithm which returns the result of invoking
  // underlyingSourceDict["start"] with argument list « controller » and
  // callback this value underlyingSource.
  startAlgorithm =
      aUnderlyingSourceDict.mStart.WasPassed()
          ? new UnderlyingSourceStartCallbackHelper(
                aUnderlyingSourceDict.mStart.Value(), aUnderlyingSource)
          : nullptr;

  // Step 6. If underlyingSourceDict["pull"] exists, then set pullAlgorithm to
  // an algorithm which returns the result of invoking
  // underlyingSourceDict["pull"] with argument list « controller » and callback
  // this value underlyingSource.
  pullAlgorithm =
      aUnderlyingSourceDict.mPull.WasPassed()
          ? new IDLUnderlyingSourcePullCallbackHelper(
                aUnderlyingSourceDict.mPull.Value(), aUnderlyingSource)
          : nullptr;

  // Step 7. If underlyingSourceDict["cancel"] exists, then set cancelAlgorithm
  // to an algorithm which takes an argument reason and returns the result of
  // invoking underlyingSourceDict["cancel"] with argument list « reason » and
  // callback this value underlyingSource.
  cancelAlgorithm =
      aUnderlyingSourceDict.mCancel.WasPassed()
          ? new IDLUnderlyingSourceCancelCallbackHelper(
                aUnderlyingSourceDict.mCancel.Value(), aUnderlyingSource)
          : nullptr;

  // Step 8. Let autoAllocateChunkSize be
  // underlyingSourceDict["autoAllocateChunkSize"], if it exists, or undefined
  // otherwise.
  Maybe<uint64_t> autoAllocateChunkSize = mozilla::Nothing();
  if (aUnderlyingSourceDict.mAutoAllocateChunkSize.WasPassed()) {
    uint64_t value = aUnderlyingSourceDict.mAutoAllocateChunkSize.Value();
    // Step 9. If autoAllocateChunkSize is 0, then throw a TypeError
    // exception.
    if (value == 0) {
      aRv.ThrowTypeError("autoAllocateChunkSize can not be zero.");
      return;
    }
    autoAllocateChunkSize = mozilla::Some(value);
  }

  // Step 10. Perform ? SetUpReadableByteStreamController(stream, controller,
  // startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
  // autoAllocateChunkSize).
  SetUpReadableByteStreamController(aCx, aStream, controller, startAlgorithm,
                                    pullAlgorithm, cancelAlgorithm,
                                    nullptr /* aErrorAlgorithm */,
                                    aHighWaterMark, autoAllocateChunkSize, aRv);
}

// This is gently modelled on the pre-existing
// SetUpExternalReadableByteStreamController, but specialized to the
// BodyStreamUnderlyingSource model vs. the External streams of the JS
// implementation.
//
// https://streams.spec.whatwg.org/#set-up-readable-byte-stream-controller-from-underlying-source
void SetUpReadableByteStreamControllerFromBodyStreamUnderlyingSource(
    JSContext* aCx, ReadableStream* aStream,
    BodyStreamHolder* aUnderlyingSource, ErrorResult& aRv) {
  // Step 1.
  RefPtr<ReadableByteStreamController> controller =
      new ReadableByteStreamController(aStream->GetParentObject());

  // Step 2.
  RefPtr<UnderlyingSourceStartCallbackHelper> startAlgorithm;

  // Step 3.
  RefPtr<UnderlyingSourcePullCallbackHelper> pullAlgorithm;

  // Step 4
  RefPtr<UnderlyingSourceCancelCallbackHelper> cancelAlgorithm;

  // Not Specified
  RefPtr<UnderlyingSourceErrorCallbackHelper> errorAlgorithm;

  // Step 5. Intentionally skipped: No startAlgorithm for
  // BodyStreamUnderlyingSources. Step 6.
  pullAlgorithm =
      new BodyStreamUnderlyingSourcePullCallbackHelper(aUnderlyingSource);

  // Step 7.
  cancelAlgorithm =
      new BodyStreamUnderlyingSourceCancelCallbackHelper(aUnderlyingSource);

  // Not Specified
  errorAlgorithm =
      new BodyStreamUnderlyingSourceErrorCallbackHelper(aUnderlyingSource);

  // Step 8
  Maybe<uint64_t> autoAllocateChunkSize = mozilla::Nothing();
  // Step 9 (Skipped)

  // Not Specified: Native underlying sources always use 0.0 high water mark.
  double highWaterMark = 0.0;

  // Step 10.
  SetUpReadableByteStreamController(
      aCx, aStream, controller, startAlgorithm, pullAlgorithm, cancelAlgorithm,
      errorAlgorithm, highWaterMark, autoAllocateChunkSize, aRv);
}

}  // namespace dom
}  // namespace mozilla
