/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReadableStream.h"
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
#include "mozilla/dom/BindingCallContext.h"
#include "mozilla/dom/ModuleMapKey.h"
#include "mozilla/dom/QueueWithSizes.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "mozilla/dom/ReadRequest.h"
#include "mozilla/dom/ReadableStreamBinding.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/dom/ReadableStreamTee.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/StreamUtils.h"
#include "mozilla/dom/TeeState.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
#include "nsCOMPtr.h"

#include "mozilla/dom/Promise-inl.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"

#include <unistd.h>

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_CLASS(ReadableStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ReadableStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal, mController, mReader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mStoredError.setNull();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ReadableStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal, mController, mReader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ReadableStream)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mStoredError)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ReadableStream)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ReadableStream)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStream)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ReadableStream::ReadableStream(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
  mozilla::HoldJSObjects(this);
}

ReadableStream::ReadableStream(const GlobalObject& aGlobal)
    : mGlobal(do_QueryInterface(aGlobal.GetAsSupports())) {
  mozilla::HoldJSObjects(this);
}

ReadableStream::~ReadableStream() { mozilla::DropJSObjects(this); }

JSObject* ReadableStream::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return ReadableStream_Binding::Wrap(aCx, this, aGivenProto);
}

void ReadableStream::SetReader(ReadableStreamDefaultReader* aReader) {
  mReader = aReader;
}

// Streams Spec: 4.2.4: https://streams.spec.whatwg.org/#rs-prototype
/* static */
already_AddRefed<ReadableStream> ReadableStream::Constructor(
    const GlobalObject& aGlobal,
    const Optional<JS::Handle<JSObject*>>& aUnderlyingSource,
    const QueuingStrategy& aStrategy, ErrorResult& aRv) {
  // Step 1.
  JS::RootedObject underlyingSourceObj(
      aGlobal.Context(),
      aUnderlyingSource.WasPassed() ? aUnderlyingSource.Value() : nullptr);

  // Step 2.
  UnderlyingSource underlyingSourceDict;
  if (underlyingSourceObj) {
    JS::RootedValue objValue(aGlobal.Context(),
                             JS::ObjectValue(*underlyingSourceObj));
    dom::BindingCallContext callCx(aGlobal.Context(),
                                   "ReadableStream.constructor");
    aRv.MightThrowJSException();
    if (!underlyingSourceDict.Init(callCx, objValue)) {
      aRv.StealExceptionFromJSContext(aGlobal.Context());
      return nullptr;
    }
  }

  // Step 3.
  RefPtr<ReadableStream> readableStream = new ReadableStream(aGlobal);

  // Step 4.
  if (underlyingSourceDict.mType.WasPassed()) {
    // Implicit assertion on above check.
    MOZ_ASSERT(underlyingSourceDict.mType.Value() == ReadableStreamType::Bytes);

    // Step 4.1
    if (aStrategy.mSize.WasPassed()) {
      aRv.ThrowRangeError("Implementation preserved member 'size'");
      return nullptr;
    }

    // Step 4.2
    double highWaterMark = ExtractHighWaterMark(aStrategy, 0, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    // Step 4.3
    (void)highWaterMark;
    MOZ_CRASH("Byte Streams Not Yet Implemented");

    return readableStream.forget();
  }

  // Step 5.1 (implicit in above check)
  // Step 5.2. Extract callback.
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

  // Step 5.3
  double highWaterMark = ExtractHighWaterMark(aStrategy, 1, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 5.4.
  SetupReadableStreamDefaultControllerFromUnderlyingSource(
      aGlobal.Context(), readableStream, underlyingSourceObj,
      underlyingSourceDict, highWaterMark, sizeAlgorithm, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return readableStream.forget();
}

// Dealing with const this ptr is a pain, so just re-implement.
// https://streams.spec.whatwg.org/#is-readable-stream-locked
bool ReadableStream::Locked() const {
  // Step 1 + 2.
  return mReader;
}

// https://streams.spec.whatwg.org/#initialize-readable-stream
static void InitializeReadableStream(ReadableStream* aStream) {
  // Step 1.
  aStream->SetState(ReadableStream::ReaderState::Readable);

  // Step 2.
  aStream->SetReader(nullptr);
  aStream->SetStoredError(JS::UndefinedHandleValue);

  // Step 3.
  aStream->SetDisturbed(false);
}

// https://streams.spec.whatwg.org/#create-readable-stream
MOZ_CAN_RUN_SCRIPT
already_AddRefed<ReadableStream> CreateReadableStream(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    UnderlyingSourceStartCallbackHelper* aStartAlgorithm,
    UnderlyingSourcePullCallbackHelper* aPullAlgorithm,
    UnderlyingSourceCancelCallbackHelper* aCancelAlgorithm,
    mozilla::Maybe<double> aHighWaterMark, QueuingStrategySize* aSizeAlgorithm,
    ErrorResult& aRv) {
  // Step 1.
  double highWaterMark = aHighWaterMark.isSome() ? *aHighWaterMark : 1.0;

  // Step 2. consumers of sizeAlgorithm
  //         handle null algorithms correctly.
  // Step 3.
  MOZ_ASSERT(IsNonNegativeNumber(highWaterMark));
  // Step 4.
  RefPtr<ReadableStream> stream = new ReadableStream(aGlobal);

  // Step 5.
  InitializeReadableStream(stream);

  // Step 6.
  RefPtr<ReadableStreamDefaultController> controller =
      new ReadableStreamDefaultController(aGlobal);

  // Step 7.
  SetUpReadableStreamDefaultController(aCx, stream, controller, aStartAlgorithm,
                                       aPullAlgorithm, aCancelAlgorithm,
                                       highWaterMark, aSizeAlgorithm, aRv);

  // Step 8.
  return stream.forget();
}

// https://streams.spec.whatwg.org/#readable-stream-close
void ReadableStreamClose(JSContext* aCx, ReadableStream* aStream,
                         ErrorResult& aRv) {
  // Step 1.
  MOZ_ASSERT(aStream->State() == ReadableStream::ReaderState::Readable);

  // Step 2.
  aStream->SetState(ReadableStream::ReaderState::Closed);

  // Step 3.

  ReadableStreamDefaultReader* reader = aStream->GetReader();
  // Step 4.
  if (!reader) {
    return;
  }

  // Step 5.
  reader->ClosedPromise()->MaybeResolveWithUndefined();

  // Step 6 (impliclitly true for now)

  // Step 6.1
  for (ReadRequest* readRequest : reader->ReadRequests()) {
    // Step 6.1.1.
    readRequest->CloseSteps(aCx, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 6.2
  reader->ReadRequests().clear();
}

// https://streams.spec.whatwg.org/#readable-stream-cancel
MOZ_CAN_RUN_SCRIPT
already_AddRefed<Promise> ReadableStreamCancel(JSContext* aCx,
                                               ReadableStream* aStream,
                                               JS::Handle<JS::Value> aError,
                                               ErrorResult& aRv) {
  // Step 1.
  aStream->SetDisturbed(true);

  // Step 2.
  if (aStream->State() == ReadableStream::ReaderState::Closed) {
    RefPtr<Promise> promise = Promise::Create(aStream->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }

  // Step 3.
  if (aStream->State() == ReadableStream::ReaderState::Errored) {
    RefPtr<Promise> promise = Promise::Create(aStream->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    JS::RootedValue storedError(aCx, aStream->StoredError());
    promise->MaybeReject(storedError);
    return promise.forget();
  }

  // Step 4.
  ReadableStreamClose(aCx, aStream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 5. Only consumed in Step 6, so skipped for now.
  // Step 6. Implicitly skipped until BYOBReaders exist

  // Step 7.
  RefPtr<Promise> sourceCancelPromise =
      aStream->Controller()->CancelSteps(aCx, aError, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 8.
  RefPtr<Promise> promise =
      Promise::Create(sourceCancelPromise->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // ThenWithCycleCollectedArgs will carry promise, keeping it alive until the
  // callback executes.
  Result<RefPtr<Promise>, nsresult> returnResult =
      sourceCancelPromise->ThenWithCycleCollectedArgs(
          [](JSContext*, JS::HandleValue, RefPtr<Promise> newPromise) {
            newPromise->MaybeResolveWithUndefined();
            return newPromise.forget();
          },
          promise);

  if (returnResult.isErr()) {
    aRv.Throw(returnResult.unwrapErr());
    return nullptr;
  }

  return returnResult.unwrap().forget();
}

MOZ_CAN_RUN_SCRIPT
already_AddRefed<Promise> ReadableStream::Cancel(JSContext* aCx,
                                                 JS::Handle<JS::Value> aReason,
                                                 ErrorResult& aRv) {
  if (Locked()) {
    RefPtr<Promise> promise = Promise::Create(GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    promise->MaybeRejectWithTypeError("Canceled Locked Stream");
    return promise.forget();
  }
  RefPtr<ReadableStream> thisRefPtr = this;
  return ReadableStreamCancel(aCx, thisRefPtr, aReason, aRv);
}

// https://streams.spec.whatwg.org/#acquire-readable-stream-reader
already_AddRefed<ReadableStreamDefaultReader>
AcquireReadableStreamDefaultReader(JSContext* aCx, ReadableStream* aStream,
                                   ErrorResult& aRv) {
  // Step 1.
  RefPtr<ReadableStreamDefaultReader> reader =
      new ReadableStreamDefaultReader(aStream->GetParentObject());

  // Step 2.
  SetUpReadableStreamDefaultReader(aCx, reader, aStream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 3.
  return reader.forget();
}

// https://streams.spec.whatwg.org/#rs-get-reader
// MG:XXX: This will need tweaking when we implement BYOBreaders
already_AddRefed<ReadableStreamDefaultReader> ReadableStream::GetReader(
    JSContext* aCx, const ReadableStreamGetReaderOptions& aOptions,
    ErrorResult& aRv) {
  // Step 1.
  if (!aOptions.mMode.WasPassed()) {
    RefPtr<ReadableStream> thisRefPtr = this;
    return AcquireReadableStreamDefaultReader(aCx, thisRefPtr, aRv);
  }
  // Step 2.
  MOZ_CRASH("BYOB STREAMS NOT IMPLEMENTED");
  return nullptr;
}

// https://streams.spec.whatwg.org/#is-readable-stream-locked
bool IsReadableStreamLocked(ReadableStream* aStream) {
  // Step 1 + 2.
  return aStream->Locked();
}

#ifdef DEBUG  // Only used in asserts until later
// https://streams.spec.whatwg.org/#readable-stream-has-default-reader
static bool ReadableStreamHasDefaultReader(ReadableStream* aStream) {
  // Step 1.
  ReadableStreamDefaultReader* reader = aStream->GetReader();

  // Step 2.
  if (!reader) {
    return false;
  }

  // Step 3. Trivially true until we implement ReadableStreamBYOBReader.
  return true;

  // Step 4. Unreachable until above.
}
#endif

// https://streams.spec.whatwg.org/#readable-stream-get-num-read-requests
double ReadableStreamGetNumReadRequests(ReadableStream* aStream) {
  // Step 1.
  MOZ_ASSERT(ReadableStreamHasDefaultReader(aStream));

  // Step 2.
  return double(aStream->GetReader()->ReadRequests().length());
}

// https://streams.spec.whatwg.org/#readable-stream-error
void ReadableStreamError(JSContext* aCx, ReadableStream* aStream,
                         JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  // Step 1.
  MOZ_ASSERT(aStream->State() == ReadableStream::ReaderState::Readable);

  // Step 2.
  aStream->SetState(ReadableStream::ReaderState::Errored);

  // Step 3.
  aStream->SetStoredError(aValue);

  // Step 4.
  ReadableStreamDefaultReader* reader = aStream->GetReader();

  // Step 5.
  if (!reader) {
    return;
  }

  // Step 6.
  reader->ClosedPromise()->MaybeReject(aValue);

  // Step 7.
  reader->ClosedPromise()->SetSettledPromiseIsHandled();

  // Step 8. Implicit in the fact that we don't yet support
  //         ReadableStreamBYOBReader

  // Step 8.1:
  for (ReadRequest* readRequest : reader->ReadRequests()) {
    readRequest->ErrorSteps(aCx, aValue, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 8.2
  reader->ReadRequests().clear();

  // Step 9: Unreachable until we implement ReadableStreamBYOBReader.
}

// https://streams.spec.whatwg.org/#rs-default-controller-close
void ReadableStreamFulfillReadRequest(JSContext* aCx, ReadableStream* aStream,
                                      JS::Handle<JS::Value> aChunk, bool aDone,
                                      ErrorResult& aRv) {
  // Step 1.
  MOZ_ASSERT(ReadableStreamHasDefaultReader(aStream));

  // Step 2.
  ReadableStreamDefaultReader* reader = aStream->GetReader();

  // Step 3.
  MOZ_ASSERT(!reader->ReadRequests().isEmpty());

  // Step 4+5.
  RefPtr<ReadRequest> readRequest = reader->ReadRequests().popFirst();

  // Step 6.
  if (aDone) {
    readRequest->CloseSteps(aCx, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 7.
  readRequest->ChunkSteps(aCx, aChunk, aRv);
}

void ReadableStreamAddReadRequest(ReadableStream* aStream,
                                  ReadRequest* aReadRequest) {
  // Step 1. Implicit until BYOB readers exist
  // Step 2.
  MOZ_ASSERT(aStream->State() == ReadableStream::ReaderState::Readable);
  // Step 3.
  aStream->GetReader()->ReadRequests().insertBack(aReadRequest);
}

class ReadableStreamDefaultTeeCancelAlgorithm final
    : public UnderlyingSourceCancelCallbackHelper {
  RefPtr<TeeState> mTeeState;
  // Since cancel1algorithm and cancel2algorithm only differ in which tee state
  // members to manipulate, we common up the implementation and select
  // dynamically.
  bool mIsCancel1 = true;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      ReadableStreamDefaultTeeCancelAlgorithm,
      UnderlyingSourceCancelCallbackHelper)

  explicit ReadableStreamDefaultTeeCancelAlgorithm(TeeState* aTeeState,
                                                   bool aIsCancel1)
      : mTeeState(aTeeState), mIsCancel1(aIsCancel1) {}

  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override {
    // Step 1.
    if (mIsCancel1) {
      mTeeState->SetCanceled1(true);
    } else {
      mTeeState->SetCanceled2(true);
    }

    // Step 2.
    if (mIsCancel1) {
      mTeeState->SetReason1(aReason.Value());
    } else {
      mTeeState->SetReason2(aReason.Value());
    }

    // Step 3.

    if ((mIsCancel1 && mTeeState->Canceled2()) ||
        (!mIsCancel1 && mTeeState->Canceled1())) {
      // Step 3.1

      JS::RootedObject compositeReason(aCx, JS::NewArrayObject(aCx, 2));
      if (!compositeReason) {
        aRv.StealExceptionFromJSContext(aCx);
        return nullptr;
      }

      JS::RootedValue reason1(aCx, mTeeState->Reason1());
      if (!JS_SetElement(aCx, compositeReason, 0, reason1)) {
        aRv.StealExceptionFromJSContext(aCx);
        return nullptr;
      }

      JS::RootedValue reason2(aCx, mTeeState->Reason2());
      if (!JS_SetElement(aCx, compositeReason, 1, reason2)) {
        aRv.StealExceptionFromJSContext(aCx);
        return nullptr;
      }

      // Step 3.2
      JS::RootedValue compositeReasonValue(aCx,
                                           JS::ObjectValue(*compositeReason));
      RefPtr<ReadableStream> stream(mTeeState->GetStream());
      RefPtr<Promise> cancelResult =
          ReadableStreamCancel(aCx, stream, compositeReasonValue, aRv);
      if (aRv.Failed()) {
        return nullptr;
      }

      // Step 3.3
      mTeeState->CancelPromise()->MaybeResolve(cancelResult);
    }

    // Step 4.
    return do_AddRef(mTeeState->CancelPromise());
  }

 protected:
  ~ReadableStreamDefaultTeeCancelAlgorithm() = default;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(ReadableStreamDefaultTeeCancelAlgorithm)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    ReadableStreamDefaultTeeCancelAlgorithm,
    UnderlyingSourceCancelCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTeeState)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    ReadableStreamDefaultTeeCancelAlgorithm,
    UnderlyingSourceCancelCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTeeState)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(ReadableStreamDefaultTeeCancelAlgorithm,
                         UnderlyingSourceCancelCallbackHelper)
NS_IMPL_RELEASE_INHERITED(ReadableStreamDefaultTeeCancelAlgorithm,
                          UnderlyingSourceCancelCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamDefaultTeeCancelAlgorithm)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourceCancelCallbackHelper)

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
// Step 18.
class ReadableStreamTeeClosePromiseHandler final : public PromiseNativeHandler {
  ~ReadableStreamTeeClosePromiseHandler() = default;
  RefPtr<TeeState> mTeeState;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ReadableStreamTeeClosePromiseHandler)

  explicit ReadableStreamTeeClosePromiseHandler(TeeState* aTeeState)
      : mTeeState(aTeeState) {}

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
  }
  void RejectedCallback(JSContext* aCx,
                        JS::Handle<JS::Value> aReason) override {
    // Step 18.1.
    ErrorResult rv;
    ReadableStreamDefaultControllerError(
        aCx, mTeeState->Branch1()->Controller(), aReason, rv);
    if (rv.MaybeSetPendingException(
            aCx, "ReadableStreamDefaultTee Error During Promise Rejection")) {
      return;
    }

    // Step 18.2
    ReadableStreamDefaultControllerError(
        aCx, mTeeState->Branch2()->Controller(), aReason, rv);
    if (rv.MaybeSetPendingException(
            aCx, "ReadableStreamDefaultTee Error During Promise Rejection")) {
      return;
    }

    // Step 18.3
    if (!mTeeState->Canceled1() || !mTeeState->Canceled2()) {
      mTeeState->CancelPromise()->MaybeResolveWithUndefined();
    }
  }
};

// Cycle collection methods for promise handler.
NS_IMPL_CYCLE_COLLECTION(ReadableStreamTeeClosePromiseHandler, mTeeState)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ReadableStreamTeeClosePromiseHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ReadableStreamTeeClosePromiseHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamTeeClosePromiseHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
MOZ_CAN_RUN_SCRIPT
static void ReadableStreamDefaultTee(JSContext* aCx, ReadableStream* aStream,
                                     bool aCloneForBranch2,
                                     nsTArray<RefPtr<ReadableStream>>& aResult,
                                     ErrorResult& aRv) {
  // Step 1. Implicit.
  // Step 2. Implicit.

  // Steps 3-11 are contained in the construction of Tee State.
  RefPtr<TeeState> teeState =
      TeeState::Create(aCx, aStream, aCloneForBranch2, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 12:
  RefPtr<ReadableStreamDefaultTeePullAlgorithm> pullAlgorithm =
      new ReadableStreamDefaultTeePullAlgorithm(teeState);

  // Link pull algorithm into tee state for use in readRequest
  teeState->SetPullAlgorithm(pullAlgorithm);

  // Step 13.
  RefPtr<UnderlyingSourceCancelCallbackHelper> cancel1Algorithm =
      new ReadableStreamDefaultTeeCancelAlgorithm(teeState, true);

  // Step 14.
  RefPtr<UnderlyingSourceCancelCallbackHelper> cancel2Algorithm =
      new ReadableStreamDefaultTeeCancelAlgorithm(teeState, false);

  // Step 15. Consumers are aware that they should return undefined
  //          in the default case for this algorithm.
  RefPtr<UnderlyingSourceStartCallbackHelper> startAlgorithm;

  // Step 16.
  nsCOMPtr<nsIGlobalObject> global(
      do_AddRef(teeState->GetStream()->GetParentObject()));
  teeState->SetBranch1(CreateReadableStream(aCx, global, startAlgorithm,
                                            pullAlgorithm, cancel1Algorithm,
                                            mozilla::Nothing(), nullptr, aRv));
  if (aRv.Failed()) {
    return;
  }

  // Step 17.
  teeState->SetBranch2(CreateReadableStream(aCx, global, startAlgorithm,
                                            pullAlgorithm, cancel2Algorithm,
                                            mozilla::Nothing(), nullptr, aRv));
  if (aRv.Failed()) {
    return;
  }

  // Step 18.
  teeState->GetReader()->ClosedPromise()->AppendNativeHandler(
      new ReadableStreamTeeClosePromiseHandler(teeState));

  // Step 19.
  aResult.AppendElement(teeState->Branch1());
  aResult.AppendElement(teeState->Branch2());
}

// https://streams.spec.whatwg.org/#readable-stream-tee
MOZ_CAN_RUN_SCRIPT
static void ReadableStreamTee(JSContext* aCx, ReadableStream* aStream,
                              bool aCloneForBranch2,
                              nsTArray<RefPtr<ReadableStream>>& aResult,
                              ErrorResult& aRv) {
  // Step 1. Implicit.
  // Step 2. Implicit.
  // Step 3. Implicitly false, until we implement
  // ReadableByteStreamController. Step 4.
  ReadableStreamDefaultTee(aCx, aStream, aCloneForBranch2, aResult, aRv);
}

MOZ_CAN_RUN_SCRIPT
void ReadableStream::Tee(JSContext* aCx,
                         nsTArray<RefPtr<ReadableStream>>& aResult,
                         ErrorResult& aRv) {
  ReadableStreamTee(aCx, this, false, aResult, aRv);
}

}  // namespace dom
}  // namespace mozilla
