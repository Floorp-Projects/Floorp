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
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/BindingCallContext.h"
#include "mozilla/dom/ByteStreamHelpers.h"
#include "mozilla/dom/BodyStream.h"
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
#include "mozilla/dom/ReadableStreamPipeTo.h"
#include "mozilla/dom/ReadableStreamTee.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/StreamUtils.h"
#include "mozilla/dom/TeeState.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/WritableStreamDefaultWriter.h"
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

namespace mozilla::dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(ReadableStream,
                                                      (mGlobal, mController,
                                                       mReader, mAlgorithms,
                                                       mNativeUnderlyingSource),
                                                      (mStoredError))

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
  JS::Rooted<JSObject*> underlyingSourceObj(
      aGlobal.Context(),
      aUnderlyingSource.WasPassed() ? aUnderlyingSource.Value() : nullptr);

  // Step 2.
  RootedDictionary<UnderlyingSource> underlyingSourceDict(aGlobal.Context());
  if (underlyingSourceObj) {
    JS::Rooted<JS::Value> objValue(aGlobal.Context(),
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
    if (!StaticPrefs::dom_streams_byte_streams_enabled()) {
      aRv.ThrowNotSupportedError("BYOB byte streams not yet supported.");
      return nullptr;
    }

    SetUpReadableByteStreamControllerFromUnderlyingSource(
        aGlobal.Context(), readableStream, underlyingSourceObj,
        underlyingSourceDict, highWaterMark, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

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
    UnderlyingSourceAlgorithmsBase* aAlgorithms,
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
  SetUpReadableStreamDefaultController(aCx, stream, controller, aAlgorithms,
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
    // Step 6.1. Let readRequests be reader.[[readRequests]].
    // Move LinkedList out of DefaultReader onto stack to avoid the potential
    // for concurrent modification, which could invalidate the iterator.
    //
    // See https://bugs.chromium.org/p/chromium/issues/detail?id=1045874 as an
    // example of the kind of issue that could occur.
    LinkedList<RefPtr<ReadRequest>> readRequests =
        std::move(reader->AsDefault()->ReadRequests());

    // Step 6.2. Set reader.[[readRequests]] to an empty list.
    // Note: The std::move already cleared this anyway.
    reader->AsDefault()->ReadRequests().clear();

    // Step 6.3. For each readRequest of readRequests,
    // Drain the local list and destroy elements along the way.
    while (RefPtr<ReadRequest> readRequest = readRequests.popFirst()) {
      // Step 6.3.1. Perform readRequest’s close steps.
      readRequest->CloseSteps(aCx, aRv);
      if (aRv.Failed()) {
        return;
      }
    }
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
    JS::Rooted<JS::Value> storedError(aCx, aStream->StoredError());
    return Promise::CreateRejected(aStream->GetParentObject(), storedError,
                                   aRv);
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
    // Step 6.1. Let readIntoRequests be reader.[[readIntoRequests]].
    LinkedList<RefPtr<ReadIntoRequest>> readIntoRequests =
        std::move(reader->AsBYOB()->ReadIntoRequests());

    // Step 6.2. Set reader.[[readIntoRequests]] to an empty list.
    // Note: The std::move already cleared this anyway.
    reader->AsBYOB()->ReadIntoRequests().clear();

    // Step 6.3. For each readIntoRequest of readIntoRequests,
    while (RefPtr<ReadIntoRequest> readIntoRequest =
               readIntoRequests.popFirst()) {
      // Step 6.3.1.Perform readIntoRequest’s close steps, given undefined.
      readIntoRequest->CloseSteps(aCx, JS::UndefinedHandleValue, aRv);
      if (aRv.Failed()) {
        return nullptr;
      }
    }
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
          [](JSContext*, JS::Handle<JS::Value>, ErrorResult&,
             RefPtr<Promise> newPromise) {
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
AcquireReadableStreamDefaultReader(ReadableStream* aStream, ErrorResult& aRv) {
  // Step 1.
  RefPtr<ReadableStreamDefaultReader> reader =
      new ReadableStreamDefaultReader(aStream->GetParentObject());

  // Step 2.
  SetUpReadableStreamDefaultReader(reader, aStream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 3.
  return reader.forget();
}

// https://streams.spec.whatwg.org/#rs-get-reader
void ReadableStream::GetReader(const ReadableStreamGetReaderOptions& aOptions,
                               OwningReadableStreamReader& resultReader,
                               ErrorResult& aRv) {
  // Step 1. If options["mode"] does not exist,
  // return ? AcquireReadableStreamDefaultReader(this).
  if (!aOptions.mMode.WasPassed()) {
    RefPtr<ReadableStream> thisRefPtr = this;
    RefPtr<ReadableStreamDefaultReader> defaultReader =
        AcquireReadableStreamDefaultReader(thisRefPtr, aRv);
    if (aRv.Failed()) {
      return;
    }
    resultReader.SetAsReadableStreamDefaultReader() = defaultReader;
    return;
  }

  // Step 2. Assert: options["mode"] is "byob".
  MOZ_ASSERT(aOptions.mMode.Value() == ReadableStreamReaderMode::Byob);

  // Step 3. Return ? AcquireReadableStreamBYOBReader(this).
  if (!StaticPrefs::dom_streams_byte_streams_enabled()) {
    aRv.ThrowTypeError("BYOB byte streams reader not yet supported.");
    return;
  }

  RefPtr<ReadableStream> thisRefPtr = this;
  RefPtr<ReadableStreamBYOBReader> byobReader =
      AcquireReadableStreamBYOBReader(thisRefPtr, aRv);
  if (aRv.Failed()) {
    return;
  }
  resultReader.SetAsReadableStreamBYOBReader() = byobReader;
}

// https://streams.spec.whatwg.org/#is-readable-stream-locked
bool IsReadableStreamLocked(ReadableStream* aStream) {
  // Step 1 + 2.
  return aStream->Locked();
}

// https://streams.spec.whatwg.org/#rs-pipe-through
MOZ_CAN_RUN_SCRIPT already_AddRefed<ReadableStream> ReadableStream::PipeThrough(
    const ReadableWritablePair& aTransform, const StreamPipeOptions& aOptions,
    ErrorResult& aRv) {
  // Step 1: If ! IsReadableStreamLocked(this) is true, throw a TypeError
  // exception.
  if (IsReadableStreamLocked(this)) {
    aRv.ThrowTypeError("Cannot pipe from a locked stream.");
    return nullptr;
  }

  // Step 2: If ! IsWritableStreamLocked(transform["writable"]) is true, throw a
  // TypeError exception.
  if (IsWritableStreamLocked(aTransform.mWritable)) {
    aRv.ThrowTypeError("Cannot pipe to a locked stream.");
    return nullptr;
  }

  // Step 3: Let signal be options["signal"] if it exists, or undefined
  // otherwise.
  RefPtr<AbortSignal> signal =
      aOptions.mSignal.WasPassed() ? &aOptions.mSignal.Value() : nullptr;

  // Step 4: Let promise be ! ReadableStreamPipeTo(this, transform["writable"],
  // options["preventClose"], options["preventAbort"], options["preventCancel"],
  // signal).
  RefPtr<WritableStream> writable = aTransform.mWritable;
  RefPtr<Promise> promise = ReadableStreamPipeTo(
      this, writable, aOptions.mPreventClose, aOptions.mPreventAbort,
      aOptions.mPreventCancel, signal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 5: Set promise.[[PromiseIsHandled]] to true.
  MOZ_ALWAYS_TRUE(promise->SetAnyPromiseIsHandled());

  // Step 6: Return transform["readable"].
  return do_AddRef(aTransform.mReadable.get());
};

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
    // Step 8.1. Perform ! ReadableStreamDefaultReaderErrorReadRequests(reader,
    // e).
    RefPtr<ReadableStreamDefaultReader> defaultReader = reader->AsDefault();
    ReadableStreamDefaultReaderErrorReadRequests(aCx, defaultReader, aValue,
                                                 aRv);
    if (aRv.Failed()) {
      return;
    }
  } else {
    // Step 9. Otherwise,
    // Step 9.1. Assert: reader implements ReadableStreamBYOBReader.
    MOZ_ASSERT(reader->IsBYOB());

    // Step 9.2. Perform ! ReadableStreamBYOBReaderErrorReadIntoRequests(reader,
    // e).
    RefPtr<ReadableStreamBYOBReader> byobReader = reader->AsBYOB();
    ReadableStreamBYOBReaderErrorReadIntoRequests(aCx, byobReader, aValue, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Not in Specification: Allow notifying native underlying sources that a
  // stream has been errored.
  if (UnderlyingSourceAlgorithmsBase* algorithms = aStream->GetAlgorithms()) {
    algorithms->ErrorCallback();
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

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
// Step 14, 15
MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise>
ReadableStreamDefaultTeeSourceAlgorithms::CancelCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  // Step 1.
  mTeeState->SetCanceled(mBranch, true);

  // Step 2.
  mTeeState->SetReason(mBranch, aReason.Value());

  // Step 3.

  if (mTeeState->Canceled(OtherTeeBranch(mBranch))) {
    // Step 3.1

    JS::Rooted<JSObject*> compositeReason(aCx, JS::NewArrayObject(aCx, 2));
    if (!compositeReason) {
      aRv.StealExceptionFromJSContext(aCx);
      return nullptr;
    }

    JS::Rooted<JS::Value> reason1(aCx, mTeeState->Reason1());
    if (!JS_SetElement(aCx, compositeReason, 0, reason1)) {
      aRv.StealExceptionFromJSContext(aCx);
      return nullptr;
    }

    JS::Rooted<JS::Value> reason2(aCx, mTeeState->Reason2());
    if (!JS_SetElement(aCx, compositeReason, 1, reason2)) {
      aRv.StealExceptionFromJSContext(aCx);
      return nullptr;
    }

    // Step 3.2
    JS::Rooted<JS::Value> compositeReasonValue(
        aCx, JS::ObjectValue(*compositeReason));
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

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
MOZ_CAN_RUN_SCRIPT
static void ReadableStreamDefaultTee(JSContext* aCx, ReadableStream* aStream,
                                     bool aCloneForBranch2,
                                     nsTArray<RefPtr<ReadableStream>>& aResult,
                                     ErrorResult& aRv) {
  // Step 1. Implicit.
  // Step 2. Implicit.

  // Steps 3-12 are contained in the construction of Tee State.
  RefPtr<TeeState> teeState = TeeState::Create(aStream, aCloneForBranch2, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 13 - 16
  auto branch1Algorithms = MakeRefPtr<ReadableStreamDefaultTeeSourceAlgorithms>(
      teeState, TeeBranch::Branch1);
  auto branch2Algorithms = MakeRefPtr<ReadableStreamDefaultTeeSourceAlgorithms>(
      teeState, TeeBranch::Branch2);

  // Step 17.
  nsCOMPtr<nsIGlobalObject> global(
      do_AddRef(teeState->GetStream()->GetParentObject()));
  teeState->SetBranch1(CreateReadableStream(aCx, global, branch1Algorithms,
                                            mozilla::Nothing(), nullptr, aRv));
  if (aRv.Failed()) {
    return;
  }

  // Step 18.
  teeState->SetBranch2(CreateReadableStream(aCx, global, branch2Algorithms,
                                            mozilla::Nothing(), nullptr, aRv));
  if (aRv.Failed()) {
    return;
  }

  // Step 19.
  teeState->GetReader()->ClosedPromise()->AddCallbacksWithCycleCollectedArgs(
      [](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv,
         TeeState* aTeeState) {},
      [](JSContext* aCx, JS::Handle<JS::Value> aReason, ErrorResult& aRv,
         TeeState* aTeeState) {
        // Step 19.1.
        ReadableStreamDefaultControllerError(
            aCx, aTeeState->Branch1()->DefaultController(), aReason, aRv);
        if (aRv.Failed()) {
          return;
        }

        // Step 19.2
        ReadableStreamDefaultControllerError(
            aCx, aTeeState->Branch2()->DefaultController(), aReason, aRv);
        if (aRv.Failed()) {
          return;
        }

        // Step 19.3
        if (!aTeeState->Canceled1() || !aTeeState->Canceled2()) {
          aTeeState->CancelPromise()->MaybeResolveWithUndefined();
        }
      },
      RefPtr(teeState));

  // Step 20.
  aResult.AppendElement(teeState->Branch1());
  aResult.AppendElement(teeState->Branch2());
}

// https://streams.spec.whatwg.org/#rs-pipe-to
already_AddRefed<Promise> ReadableStream::PipeTo(
    WritableStream& aDestination, const StreamPipeOptions& aOptions,
    ErrorResult& aRv) {
  // Step 1. If !IsReadableStreamLocked(this) is true, return a promise rejected
  // with a TypeError exception.
  if (IsReadableStreamLocked(this)) {
    aRv.ThrowTypeError("Cannot pipe from a locked stream.");
    return nullptr;
  }

  // Step 2. If !IsWritableStreamLocked(destination) is true, return a promise
  //         rejected with a TypeError exception.
  if (IsWritableStreamLocked(&aDestination)) {
    aRv.ThrowTypeError("Cannot pipe to a locked stream.");
    return nullptr;
  }

  // Step 3. Let signal be options["signal"] if it exists, or undefined
  // otherwise.
  RefPtr<AbortSignal> signal =
      aOptions.mSignal.WasPassed() ? &aOptions.mSignal.Value() : nullptr;

  // Step 4. Return ! ReadableStreamPipeTo(this, destination,
  // options["preventClose"], options["preventAbort"], options["preventCancel"],
  // signal).
  return ReadableStreamPipeTo(this, &aDestination, aOptions.mPreventClose,
                              aOptions.mPreventAbort, aOptions.mPreventCancel,
                              signal, aRv);
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
    UnderlyingSourceAlgorithmsBase* aAlgorithms, ErrorResult& aRv) {
  // Step 1. Let stream be a new ReadableStream.
  RefPtr<ReadableStream> stream = new ReadableStream(aGlobal);

  // Step 2. Perform ! InitializeReadableStream(stream).
  InitializeReadableStream(stream);

  // Step 3. Let controller be a new ReadableByteStreamController.
  RefPtr<ReadableByteStreamController> controller =
      new ReadableByteStreamController(aGlobal);

  // Step 4. Perform ? SetUpReadableByteStreamController(stream, controller,
  // startAlgorithm, pullAlgorithm, cancelAlgorithm, 0, undefined).
  SetUpReadableByteStreamController(aCx, stream, controller, aAlgorithms, 0,
                                    mozilla::Nothing(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Return stream.
  return stream.forget();
}

already_AddRefed<ReadableStream> ReadableStream::Create(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    BodyStreamHolder* aUnderlyingSource, ErrorResult& aRv) {
  RefPtr<ReadableStream> stream = new ReadableStream(aGlobal);

  stream->SetNativeUnderlyingSource(aUnderlyingSource);

  SetUpReadableByteStreamControllerFromBodyStreamUnderlyingSource(
      aCx, stream, aUnderlyingSource, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 5. Return stream.
  return stream.forget();
}

}  // namespace mozilla::dom
