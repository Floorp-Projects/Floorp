/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReadableStream.h"
#include "js/Array.h"
#include "js/Exception.h"
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
#include "mozilla/dom/ByteStreamHelpers.h"
#include "mozilla/dom/BodyStream.h"
#include "mozilla/dom/ModuleMapKey.h"
#include "mozilla/dom/QueueWithSizes.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "mozilla/dom/ReadIntoRequest.h"
#include "mozilla/dom/ReadRequest.h"
#include "mozilla/dom/ReadableByteStreamController.h"
#include "mozilla/dom/ReadableStreamBYOBReader.h"
#include "mozilla/dom/ReadableStreamBinding.h"
#include "mozilla/dom/ReadableStreamController.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/dom/ReadableStreamTee.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/StreamUtils.h"
#include "mozilla/dom/TeeState.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "nsCOMPtr.h"

#include "mozilla/dom/Promise-inl.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    mozilla::Variant<mozilla::Nothing,
                     RefPtr<mozilla::dom::ReadableStreamDefaultReader>>&
        aReader,
    const char* aName, uint32_t aFlags = 0) {
  if (aReader.is<RefPtr<mozilla::dom::ReadableStreamDefaultReader>>()) {
    ImplCycleCollectionTraverse(
        aCallback,
        aReader.as<RefPtr<mozilla::dom::ReadableStreamDefaultReader>>(), aName,
        aFlags);
  }
}

inline void ImplCycleCollectionUnlink(
    mozilla::Variant<mozilla::Nothing,
                     RefPtr<mozilla::dom::ReadableStreamDefaultReader>>&
        aReader) {
  aReader = AsVariant(mozilla::Nothing());
}

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_CLASS(ReadableStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ReadableStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal, mController, mReader,
                                  mErrorAlgorithm, mNativeUnderlyingSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mStoredError.setNull();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ReadableStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal, mController, mReader,
                                    mErrorAlgorithm, mNativeUnderlyingSource)
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

ReadableStream::ReadableStream(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal), mReader(nullptr) {
  mozilla::HoldJSObjects(this);
}

ReadableStream::ReadableStream(const GlobalObject& aGlobal)
    : mGlobal(do_QueryInterface(aGlobal.GetAsSupports())), mReader(nullptr) {
  mozilla::HoldJSObjects(this);
}

ReadableStream::~ReadableStream() { mozilla::DropJSObjects(this); }

JSObject* ReadableStream::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return ReadableStream_Binding::Wrap(aCx, this, aGivenProto);
}

ReadableStreamDefaultReader* ReadableStream::GetDefaultReader() {
  return mReader->AsDefault();
}

void ReadableStream::SetReader(ReadableStreamGenericReader* aReader) {
  mReader = aReader;
}

// https://streams.spec.whatwg.org/#readable-stream-has-byob-reader
bool ReadableStreamHasBYOBReader(ReadableStream* aStream) {
  // Step 1. Let reader be stream.[[reader]].
  ReadableStreamGenericReader* reader = aStream->GetReader();

  // Step 2. If reader is undefined, return false.
  if (!reader) {
    return false;
  }

  // Step 3. If reader implements ReadableStreamBYOBReader, return true.
  // Step 4. Return false.
  return reader->IsBYOB();
}

// https://streams.spec.whatwg.org/#readable-stream-has-default-reader
bool ReadableStreamHasDefaultReader(ReadableStream* aStream) {
  // Step 1. Let reader be stream.[[reader]].
  ReadableStreamGenericReader* reader = aStream->GetReader();

  // Step 2. If reader is undefined, return false.
  if (!reader) {
    return false;
  }

  // Step 3. If reader implements ReadableStreamDefaultReader, return true.
  // Step 4. Return false.
  return reader->IsDefault();
}

void ReadableStream::SetNativeUnderlyingSource(
    BodyStreamHolder* aUnderlyingSource) {
  mNativeUnderlyingSource = aUnderlyingSource;
}

void ReadableStream::ReleaseObjects() {
  SetNativeUnderlyingSource(nullptr);

  SetErrorAlgorithm(nullptr);

  if (mController->IsByte()) {
    ReadableByteStreamControllerClearAlgorithms(mController->AsByte());
    return;
  }

  MOZ_ASSERT(mController->IsDefault());
  ReadableStreamDefaultControllerClearAlgorithms(mController->AsDefault());
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
    aRv.ThrowNotSupportedError("BYOB Byte Streams Not Yet Supported");

    return nullptr;
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
  ReadableStreamGenericReader* reader = aStream->GetReader();

  // Step 4.
  if (!reader) {
    return;
  }

  // Step 5.
  reader->ClosedPromise()->MaybeResolveWithUndefined();

  // Step 6.
  if (reader->IsDefault()) {
    // Step 6.1
    ReadableStreamDefaultReader* defaultReader = reader->AsDefault();
    for (ReadRequest* readRequest : defaultReader->ReadRequests()) {
      // Step 6.1.1.
      readRequest->CloseSteps(aCx, aRv);
      if (aRv.Failed()) {
        return;
      }
    }

    // Step 6.2
    defaultReader->ReadRequests().clear();
  }
}

// https://streams.spec.whatwg.org/#readable-stream-cancel
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

  // Step 5.
  ReadableStreamGenericReader* reader = aStream->GetReader();

  // Step 6.
  if (reader && reader->IsBYOB()) {
    // Step 6.1.
    for (RefPtr<ReadIntoRequest> readIntoRequest :
         reader->AsBYOB()->ReadIntoRequests()) {
      readIntoRequest->CloseSteps(aCx, JS::UndefinedHandleValue, aRv);
      if (aRv.Failed()) {
        return nullptr;
      }
    }

    // Step 6.2.
    reader->AsBYOB()->ReadIntoRequests().clear();
  }

  // Step 7.
  RefPtr<ReadableStreamController> controller(aStream->Controller());
  RefPtr<Promise> sourceCancelPromise =
      controller->CancelSteps(aCx, aError, aRv);
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

// https://streams.spec.whatwg.org/#rs-cancel
already_AddRefed<Promise> ReadableStream::Cancel(JSContext* aCx,
                                                 JS::Handle<JS::Value> aReason,
                                                 ErrorResult& aRv) {
  // Step 1. If ! IsReadableStreamLocked(this) is true,
  // return a promise rejected with a TypeError exception.
  if (Locked()) {
    aRv.ThrowTypeError("Cannot cancel a stream locked by a reader.");
    return nullptr;
  }

  // Step 2. Return ! ReadableStreamCancel(this, reason).
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
void ReadableStream::GetReader(JSContext* aCx,
                               const ReadableStreamGetReaderOptions& aOptions,
                               OwningReadableStreamReader& resultReader,
                               ErrorResult& aRv) {
  // Step 1.
  if (!aOptions.mMode.WasPassed()) {
    RefPtr<ReadableStream> thisRefPtr = this;
    RefPtr<ReadableStreamDefaultReader> defaultReader =
        AcquireReadableStreamDefaultReader(aCx, thisRefPtr, aRv);
    if (aRv.Failed()) {
      return;
    }
    resultReader.SetAsReadableStreamDefaultReader() = defaultReader;
    return;
  }
  // Step 2.
  aRv.ThrowTypeError("BYOB STREAMS NOT IMPLEMENTED");
}

// https://streams.spec.whatwg.org/#is-readable-stream-locked
bool IsReadableStreamLocked(ReadableStream* aStream) {
  // Step 1 + 2.
  return aStream->Locked();
}

// https://streams.spec.whatwg.org/#readable-stream-get-num-read-requests
double ReadableStreamGetNumReadRequests(ReadableStream* aStream) {
  // Step 1.
  MOZ_ASSERT(ReadableStreamHasDefaultReader(aStream));

  // Step 2.
  return double(aStream->GetDefaultReader()->ReadRequests().length());
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
  ReadableStreamGenericReader* reader = aStream->GetReader();

  // Step 5.
  if (!reader) {
    return;
  }

  // Step 6.
  reader->ClosedPromise()->MaybeReject(aValue);

  // Step 7.
  reader->ClosedPromise()->SetSettledPromiseIsHandled();

  // Step 8.
  if (reader->IsDefault()) {
    // Step 8.1:
    ReadableStreamDefaultReader* defaultReader = reader->AsDefault();
    for (ReadRequest* readRequest : defaultReader->ReadRequests()) {
      readRequest->ErrorSteps(aCx, aValue, aRv);
      if (aRv.Failed()) {
        return;
      }
    }
    // Step 8.2
    defaultReader->ReadRequests().clear();
  } else {
    // Step 9.
    // Step 9.1.
    MOZ_ASSERT(reader->IsBYOB());
    ReadableStreamBYOBReader* byobReader = reader->AsBYOB();
    // Step 9.2.
    for (auto* readIntoRequest : byobReader->ReadIntoRequests()) {
      readIntoRequest->ErrorSteps(aCx, aValue, aRv);
      if (aRv.Failed()) {
        return;
      }
    }
    // Step 9.3
    byobReader->ReadIntoRequests().clear();
  }

  // Not in Specification: Allow notifying native underlying sources that a
  // stream has been errored.
  if (aStream->GetErrorAlgorithm()) {
    aStream->GetErrorAlgorithm()->Call();
  }
}

// https://streams.spec.whatwg.org/#rs-default-controller-close
void ReadableStreamFulfillReadRequest(JSContext* aCx, ReadableStream* aStream,
                                      JS::Handle<JS::Value> aChunk, bool aDone,
                                      ErrorResult& aRv) {
  // Step 1.
  MOZ_ASSERT(ReadableStreamHasDefaultReader(aStream));

  // Step 2.
  ReadableStreamDefaultReader* reader = aStream->GetDefaultReader();

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

// https://streams.spec.whatwg.org/#readable-stream-add-read-request
void ReadableStreamAddReadRequest(ReadableStream* aStream,
                                  ReadRequest* aReadRequest) {
  // Step 1.
  MOZ_ASSERT(aStream->GetReader()->IsDefault());
  // Step 2.
  MOZ_ASSERT(aStream->State() == ReadableStream::ReaderState::Readable);
  // Step 3.
  aStream->GetDefaultReader()->ReadRequests().insertBack(aReadRequest);
}

class ReadableStreamDefaultTeeCancelAlgorithm final
    : public UnderlyingSourceCancelCallbackHelper {
  RefPtr<TeeState> mTeeState;
  // Since cancel1algorithm and cancel2algorithm only differ in which tee
  // state members to manipulate, we common up the implementation and select
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
// Step 19.
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
    // Step 19.1.
    ErrorResult rv;
    ReadableStreamDefaultControllerError(
        aCx, mTeeState->Branch1()->DefaultController(), aReason, rv);
    if (rv.MaybeSetPendingException(
            aCx, "ReadableStreamDefaultTee Error During Promise Rejection")) {
      return;
    }

    // Step 19.2
    ReadableStreamDefaultControllerError(
        aCx, mTeeState->Branch2()->DefaultController(), aReason, rv);
    if (rv.MaybeSetPendingException(
            aCx, "ReadableStreamDefaultTee Error During Promise Rejection")) {
      return;
    }

    // Step 19.3
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

  // Steps 3-12 are contained in the construction of Tee State.
  RefPtr<TeeState> teeState =
      TeeState::Create(aCx, aStream, aCloneForBranch2, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 13:
  RefPtr<ReadableStreamDefaultTeePullAlgorithm> pullAlgorithm =
      new ReadableStreamDefaultTeePullAlgorithm(teeState);

  // Link pull algorithm into tee state for use in readRequest
  teeState->SetPullAlgorithm(pullAlgorithm);

  // Step 14.
  RefPtr<UnderlyingSourceCancelCallbackHelper> cancel1Algorithm =
      new ReadableStreamDefaultTeeCancelAlgorithm(teeState, true);

  // Step 15.
  RefPtr<UnderlyingSourceCancelCallbackHelper> cancel2Algorithm =
      new ReadableStreamDefaultTeeCancelAlgorithm(teeState, false);

  // Step 16. Consumers are aware that they should return undefined
  //          in the default case for this algorithm.
  RefPtr<UnderlyingSourceStartCallbackHelper> startAlgorithm;

  // Step 17.
  nsCOMPtr<nsIGlobalObject> global(
      do_AddRef(teeState->GetStream()->GetParentObject()));
  teeState->SetBranch1(CreateReadableStream(aCx, global, startAlgorithm,
                                            pullAlgorithm, cancel1Algorithm,
                                            mozilla::Nothing(), nullptr, aRv));
  if (aRv.Failed()) {
    return;
  }

  // Step 18.
  teeState->SetBranch2(CreateReadableStream(aCx, global, startAlgorithm,
                                            pullAlgorithm, cancel2Algorithm,
                                            mozilla::Nothing(), nullptr, aRv));
  if (aRv.Failed()) {
    return;
  }

  // Step 19.
  teeState->GetReader()->ClosedPromise()->AppendNativeHandler(
      new ReadableStreamTeeClosePromiseHandler(teeState));

  // Step 20.
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
  // Step 3.
  if (aStream->Controller()->IsByte()) {
    ReadableByteStreamTee(aCx, aStream, aResult, aRv);
    return;
  }
  // Step 4.
  ReadableStreamDefaultTee(aCx, aStream, aCloneForBranch2, aResult, aRv);
}

void ReadableStream::Tee(JSContext* aCx,
                         nsTArray<RefPtr<ReadableStream>>& aResult,
                         ErrorResult& aRv) {
  ReadableStreamTee(aCx, this, false, aResult, aRv);
}

// https://streams.spec.whatwg.org/#readable-stream-add-read-into-request
void ReadableStreamAddReadIntoRequest(ReadableStream* aStream,
                                      ReadIntoRequest* aReadIntoRequest) {
  // Step 1. Assert: stream.[[reader]] implements ReadableStreamBYOBReader.
  MOZ_ASSERT(aStream->GetReader()->IsBYOB());

  // Step 2. Assert: stream.[[state]] is "readable" or "closed".
  MOZ_ASSERT(aStream->State() == ReadableStream::ReaderState::Readable ||
             aStream->State() == ReadableStream::ReaderState::Closed);

  // Step 3. Append readRequest to stream.[[reader]].[[readIntoRequests]].
  aStream->GetReader()->AsBYOB()->ReadIntoRequests().insertBack(
      aReadIntoRequest);
}

// https://streams.spec.whatwg.org/#abstract-opdef-createreadablebytestream
already_AddRefed<ReadableStream> CreateReadableByteStream(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    UnderlyingSourceStartCallbackHelper* aStartAlgorithm,
    UnderlyingSourcePullCallbackHelper* aPullAlgorithm,
    UnderlyingSourceCancelCallbackHelper* aCancelAlgorithm, ErrorResult& aRv) {
  // Step 1. Let stream be a new ReadableStream.
  RefPtr<ReadableStream> stream = new ReadableStream(aGlobal);

  // Step 2. Perform ! InitializeReadableStream(stream).
  InitializeReadableStream(stream);

  // Step 3. Let controller be a new ReadableByteStreamController.
  RefPtr<ReadableByteStreamController> controller =
      new ReadableByteStreamController(aGlobal);

  // Step 4. Perform ? SetUpReadableByteStreamController(stream, controller,
  // startAlgorithm, pullAlgorithm, cancelAlgorithm, 0, undefined).
  SetUpReadableByteStreamController(aCx, stream, controller, aStartAlgorithm,
                                    aPullAlgorithm, aCancelAlgorithm, nullptr,
                                    0, mozilla::Nothing(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Return stream.
  return stream.forget();
}

MOZ_CAN_RUN_SCRIPT
already_AddRefed<ReadableStream> ReadableStream::Create(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    BodyStreamHolder* aUnderlyingSource, ErrorResult& aRv) {
  RefPtr<ReadableStream> stream = new ReadableStream(aGlobal);

  stream->SetNativeUnderlyingSource(aUnderlyingSource);

  SetUpReadableByteStreamControllerFromUnderlyingSource(aCx, stream,
                                                        aUnderlyingSource, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 5. Return stream.
  return stream.forget();
}

}  // namespace dom
}  // namespace mozilla
