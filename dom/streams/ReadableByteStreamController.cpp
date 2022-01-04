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
#include "mozilla/dom/ByteStreamHelpers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ReadableByteStreamController.h"
#include "mozilla/dom/ReadableByteStreamControllerBinding.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamBYOBRequest.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"

#include <algorithm>  // std::min

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(ReadableByteStreamController)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ReadableByteStreamController,
                                                ReadableStreamController)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mByobRequest, mStream)
  tmp->ClearPendingPullIntos();
  tmp->ClearQueue();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ReadableByteStreamController,
                                                  ReadableStreamController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mByobRequest, mStream)
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
    : ReadableStreamController(aGlobal) {}

ReadableByteStreamController::~ReadableByteStreamController() {
  ClearPendingPullIntos();
  ClearQueue();
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
    JSContext* aCx, ReadableByteStreamController* aController) {
  // Step 1.
  if (!aController->GetByobRequest() &&
      !aController->PendingPullIntos().isEmpty()) {
    // Step 1.1:
    PullIntoDescriptor* firstDescriptor =
        aController->PendingPullIntos().getFirst();

    // Step 1.2:
    JS::Rooted<JSObject*> buffer(aCx, firstDescriptor->Buffer());
    JS::Rooted<JSObject*> view(
        aCx, JS_NewUint8ArrayWithBuffer(
                 aCx, buffer,
                 firstDescriptor->ByteOffset() + firstDescriptor->BytesFilled(),
                 int64_t(firstDescriptor->ByteLength() -
                         firstDescriptor->BytesFilled())));

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
ReadableByteStreamController::GetByobRequest(JSContext* aCx) {
  return ReadableByteStreamControllerGetBYOBRequest(aCx, this);
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
  ReadableStream* stream = aController->Stream();

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

#if 0
  // Step 6: Disabled until BYOB Streams implemented
  if (ReadableStreamHasBYOBReader(stream) && ReadableStreamGetNumR)
#endif

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

void ReadableByteStreamControllerCallPullIfNeeded(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv);

// MG:XXX: There's a template hiding here for handling the difference between
// default and byte stream, eventually?
class ByteStreamPullIfNeededPromiseHandler final : public PromiseNativeHandler {
  ~ByteStreamPullIfNeededPromiseHandler() = default;

  RefPtr<ReadableByteStreamController> mController;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ByteStreamPullIfNeededPromiseHandler)

  explicit ByteStreamPullIfNeededPromiseHandler(
      ReadableByteStreamController* aController)
      : PromiseNativeHandler(), mController(aController) {}

  MOZ_CAN_RUN_SCRIPT
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    // https://streams.spec.whatwg.org/#readable-byte-stream-controller-call-pull-if-needed
    // Step 7.1
    mController->SetPulling(false);
    // Step 7.2
    if (mController->PullAgain()) {
      // Step 7.2.1
      mController->SetPullAgain(false);

      // Step 7.2.2
      ErrorResult rv;
      ReadableByteStreamControllerCallPullIfNeeded(aCx, mController, rv);
      (void)rv.MaybeSetPendingException(aCx, "PullIfNeeded Resolved Error");
    }
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    // https://streams.spec.whatwg.org/#readable-byte-stream-controller-call-pull-if-needed
    // Step 8.1
    ErrorResult rv;
    ReadableByteStreamControllerError(mController, aValue, rv);
    (void)rv.MaybeSetPendingException(aCx, "PullIfNeeded Rejected Error");
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
MOZ_CAN_RUN_SCRIPT
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

  // Steps 7+8
  RefPtr<ByteStreamPullIfNeededPromiseHandler> promiseHandler =
      new ByteStreamPullIfNeededPromiseHandler(aController);
  pullPromise->AppendNativeHandler(promiseHandler);
}

already_AddRefed<PullIntoDescriptor>
ReadableByteStreamControllerShiftPendingPullInto(
    ReadableByteStreamController* aController);

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-enqueue
MOZ_CAN_RUN_SCRIPT
void ReadableByteStreamControllerEnqueue(
    JSContext* aCx, ReadableByteStreamController* aController,
    JS::Handle<JSObject*> aChunk, ErrorResult& aRv) {
  // Step 1.
  ReadableStream* stream = aController->Stream();

  // Step 2.
  if (aController->CloseRequested() ||
      stream->State() != ReadableStream::ReaderState::Readable) {
    return;
  }

  // Step 3.
  bool isShared;
  JS::Rooted<JSObject*> buffer(
      aCx, JS_GetArrayBufferViewBuffer(aCx, aChunk, &isShared));

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
    PullIntoDescriptor* firstPendingPullInto =
        aController->PendingPullIntos().getFirst();

    // Step 8.2
    JS::Rooted<JSObject*> pendingBuffer(aCx, firstPendingPullInto->Buffer());
    if (JS::IsDetachedArrayBufferObject(pendingBuffer)) {
      aRv.ThrowTypeError("Pending PullInto has detatched buffer");
      return;
    }

    // Step 8.3
    pendingBuffer = TransferArrayBuffer(aCx, pendingBuffer);
    if (!pendingBuffer) {
      aRv.StealExceptionFromJSContext(aCx);
      return;
    }
    firstPendingPullInto->SetBuffer(pendingBuffer);
  }

  // Step 9.
  ReadableByteStreamControllerInvalidateBYOBRequest(aController);

  // Step 10.
  if (ReadableStreamHasDefaultReader(stream)) {
    // Step 10.1
    if (ReadableStreamGetNumReadRequests(stream) == 0) {
      // Step 10.1.1
      MOZ_ASSERT(aController->PendingPullIntos().isEmpty());

      // Step 10.1.2.
      ReadableByteStreamControllerEnqueueChunkToQueue(
          aController, transferredBuffer, byteOffset, byteLength);
    } else {
      // Step 10.2.1
      MOZ_ASSERT(aController->Queue().isEmpty());

      // Step 10.2.2
      if (!aController->PendingPullIntos().isEmpty()) {
        // Step 10.2.2.1:
        MOZ_ASSERT(
            aController->PendingPullIntos().getFirst()->GetReaderType() ==
            ReaderType::Default);

        // Step 10.2.2.2:
        (void)ReadableByteStreamControllerShiftPendingPullInto(aController);
      }

      // Step 10.2.3
      JS::Rooted<JSObject*> transferredView(
          aCx, JS_NewUint8ArrayWithBuffer(aCx, transferredBuffer, byteOffset,
                                          int64_t(byteLength)));
      if (!transferredView) {
        aRv.StealExceptionFromJSContext(aCx);
        return;
      }

      // Step 10.2.4
      JS::RootedValue transferredViewValue(aCx,
                                           JS::ObjectValue(*transferredView));
      ReadableStreamFulfillReadRequest(aCx, stream, transferredViewValue, false,
                                       aRv);
      if (aRv.Failed()) {
        return;
      }
    }
    // Step 11
  } else if (ReadableStreamHasBYOBReader(stream)) {
    MOZ_CRASH("MG:XXX:NYI -- BYOBReaders");
  } else {
    // Step 12.1
    MOZ_ASSERT(IsReadableStreamLocked(stream));

    // Step 12.2
    ReadableByteStreamControllerEnqueueChunkToQueue(
        aController, transferredBuffer, byteOffset, byteLength);
  }

  // Step 13.
  ReadableByteStreamControllerCallPullIfNeeded(aCx, aController, aRv);
}

// https://streams.spec.whatwg.org/#rbs-controller-enqueue
MOZ_CAN_RUN_SCRIPT
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
MOZ_CAN_RUN_SCRIPT
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
MOZ_CAN_RUN_SCRIPT
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
    ReadableStreamClose(aCx, aController->Stream(), aRv);
    return;
  }

  // Step 3.1
  ReadableByteStreamControllerCallPullIfNeeded(aCx, aController, aRv);
}

// https://streams.spec.whatwg.org/#rbs-controller-private-pull
MOZ_CAN_RUN_SCRIPT
void ReadableByteStreamController::PullSteps(JSContext* aCx,
                                             ReadRequest* aReadRequest,
                                             ErrorResult& aRv) {
  // Step 1.
  ReadableStream* stream = Stream();

  // Step 2.
  MOZ_ASSERT(ReadableStreamHasDefaultReader(stream));

  // Step 3.
  if (QueueTotalSize() > 0) {
    // Step 3.1
    MOZ_ASSERT(ReadableStreamGetNumReadRequests(stream) == 0);

    // Step 3.2 + 3.3
    RefPtr<ReadableByteStreamQueueEntry> entry = Queue().popFirst();

    // Step 3.4
    SetQueueTotalSize(QueueTotalSize() - double(entry->ByteLength()));

    // Step 3.5
    ReadableByteStreamControllerHandleQueueDrain(aCx, this, aRv);
    if (aRv.Failed()) {
      return;
    }

    // Step 3.6
    JS::Rooted<JSObject*> buffer(aCx, entry->Buffer());
    JS::Rooted<JSObject*> view(
        aCx, JS_NewUint8ArrayWithBuffer(aCx, buffer, entry->ByteOffset(),
                                        int64_t(entry->ByteLength())));

    // Step 3.7
    JS::RootedValue viewValue(aCx, JS::ObjectValue(*view));
    aReadRequest->ChunkSteps(aCx, viewValue, aRv);
    if (aRv.Failed()) {
      return;
    }

    // Step 3.8
    return;
  }

  // Step 4.
  Maybe<uint64_t> autoAllocateChunkSize = AutoAllocateChunkSize();

  // Step 5.
  if (autoAllocateChunkSize) {
    // Step 5.1
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

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-convert-pull-into-descriptor
JSObject* ReadableByteStreamControllerConvertPullIntoDescriptor(
    JSContext* aCx, RefPtr<PullIntoDescriptor>& pullIntoDescriptor,
    ErrorResult& aRv) {
  // Step 1.
  uint64_t bytesFilled = pullIntoDescriptor->BytesFilled();

  // Step2.
  uint64_t elementSize = pullIntoDescriptor->ElementSize();

  // Step 3.
  MOZ_ASSERT(bytesFilled <= pullIntoDescriptor->ByteLength());

  // Step 4.
  MOZ_ASSERT(bytesFilled % elementSize == 0);

  // Step 5.
  JS::Rooted<JSObject*> srcBuffer(aCx, pullIntoDescriptor->Buffer());
  JS::Rooted<JSObject*> buffer(aCx, TransferArrayBuffer(aCx, srcBuffer));
  if (!buffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return nullptr;
  }

  // Step 6.
  JS::Rooted<JSObject*> res(aCx);

  switch (pullIntoDescriptor->ViewConstructor()) {
    case PullIntoDescriptor::Constructor::DataView:
      res = JS_NewDataView(aCx, buffer, pullIntoDescriptor->ByteOffset(),
                           bytesFilled % elementSize);
      break;

#define CONSTRUCT_TYPED_ARRAY_TYPE(ExternalT, NativeT, Name)                 \
  case PullIntoDescriptor::Constructor::Name:                                \
    res = JS_New##Name##ArrayWithBuffer(aCx, buffer,                         \
                                        pullIntoDescriptor->ByteOffset(),    \
                                        int64_t(bytesFilled % elementSize)); \
    break;

      JS_FOR_EACH_TYPED_ARRAY(CONSTRUCT_TYPED_ARRAY_TYPE)

#undef CONSTRUCT_TYPED_ARRAY_TYPE
  }

  if (!res) {
    aRv.StealExceptionFromJSContext(aCx);
    return nullptr;
  }

  return res;
}

#if 0
// https://streams.spec.whatwg.org/#readable-stream-fulfill-read-into-request
void ReadableStreamFulfillReadIntoRequest(JSContext* aCx,
                                          RefPtr<ReadableStream>& aStream,
                                         <JS:: JS::>HandleValue aChunk, bool done,
                                          ErrorResult& aRv) {
  // Step 1.
  MOZ_ASSERT(ReadableStreamHasBYOBReader(aStream));

  // Step 2.
  RefPtr<ReadableStreamBYOBReader> reader =
      aStream->mReader.as<RefPtr<ReadableStreamBYOBReader>>();

  // Step 3.
  MOZ_ASSERT(!reader->mReadIntoRequests.isEmpty());

  // Step 4.
  RefPtr<ReadIntoRequest> readIntoRequest =
      reader->mReadIntoRequests.popFirst();

  // Step 5.
  if (done) {
    readIntoRequest->closeSteps(aCx, aChunk, aRv);
    return;
  }

  // Step 6.
  readIntoRequest->chunkSteps(aCx, aChunk, aRv);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-commit-pull-into-descriptor
void ReadableByteStreamControllerCommitPullIntoDescriptor(
    JSContext* aCx, RefPtr<ReadableStream>& aStream,
    RefPtr<PullIntoDescriptor>& pullIntoDescriptor, ErrorResult& aRv) {
  // Step 1.
  MOZ_ASSERT(aStream->state() != ReadableStream::ReaderState::Errored);

  // Step 2.
  bool done = false;

  // Step 3.
  if (aStream->state() == ReadableStream::ReaderState::Closed) {
    // Step 3.1
    MOZ_ASSERT(pullIntoDescriptor->bytesFilled() == 0);
    done = true;
  }

  // Step 4.
  JS::Rooted<JSObject*> filledView(
      aCx, ReadableByteStreamControllerConvertPullIntoDescriptor(
               aCx, pullIntoDescriptor, aRv));
  if (aRv.Failed()) {
    return;
  }
  JS::RootedValue filledViewValue(aCx, JS::ObjectValue(*filledView));

  // Step 5.
  if (pullIntoDescriptor->readerType() == ReaderType::Default) {
    ReadableStreamFulfillReadRequest(aCx, aStream, filledViewValue, done,
                                     aRv);
    return;
  }

  // Step 6.1
  MOZ_ASSERT(pullIntoDescriptor->readerType() == ReaderType::BYOB);
  ReadableStreamFulfillReadIntoRequest(aCx, aStream, filledViewValue, done,
                                       aRv);
}
#endif

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-in-closed-state
static void ReadableByteStreamControllerRespondInClosedState(
    JSContext* aCx, ReadableByteStreamController* aController,
    RefPtr<PullIntoDescriptor>& aFirstDescriptor, ErrorResult& aRv) {
  // Step 1.
  MOZ_ASSERT(aFirstDescriptor->BytesFilled() == 0);

  // Step 2.
  ReadableStream* stream = aController->Stream();

  // Step 3.
  if (ReadableStreamHasBYOBReader(stream)) {
    MOZ_CRASH("MG:XXX:NYI -- BYOBReader");
#if 0
    // Step 3.1
    while (ReadableStreamGetNumReadIntoRequests(stream) > 0) {
      // Step 3.1.1
      RefPtr<PullIntoDescriptor> pullIntoDescriptor =
          ReadableByteStreamControllerShiftPendingPullInto(aController);

      // Step 3.1.2.
      ReadableByteStreamControllerCommitPullIntoDescriptor(
          aCx, stream, pullIntoDescriptor, aRv);
      MOZ_CRASH("MG:XXX:VERITY NOT MORE STEPS?");
    }
#endif
  }
}

static void ReadableByteStreamControllerRespondInReadableState(
    JSContext* aCx, ReadableByteStreamController* aController,
    uint64_t aBytesWritten, PullIntoDescriptor* aPullIntoDescriptor,
    ErrorResult& aRv) {
  // MG:XXX: This needs to be finished when
  // ReadableByteStreamControllerRespondInClosedState is finished.
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-internal
MOZ_CAN_RUN_SCRIPT
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
MOZ_CAN_RUN_SCRIPT
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
MOZ_CAN_RUN_SCRIPT
void ReadableByteStreamControllerRespondWithNewView(
    JSContext* aCx, ReadableByteStreamController* aController,
    JS::Handle<JSObject*> aView, ErrorResult& aRv) {
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

  // Step 10.
  JS::Rooted<JSObject*> transferedBuffer(aCx, TransferArrayBuffer(aCx, aView));
  if (!transferedBuffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  firstDescriptor->SetBuffer(transferedBuffer);

  // Step 11.
  ReadableByteStreamControllerRespondInternal(
      aCx, aController, JS_GetArrayBufferViewByteLength(aView), aRv);
}

}  // namespace dom
}  // namespace mozilla
