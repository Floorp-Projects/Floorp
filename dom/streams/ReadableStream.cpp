/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReadableStream.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/Attributes.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/BindingCallContext.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "mozilla/dom/ReadableStreamBinding.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
#include "nsCOMPtr.h"

#include "mozilla/dom/Promise-inl.h"

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

// FIXME: This needs to go into a helper file.
// Streams Spec: 7.4
// https://streams.spec.whatwg.org/#validate-and-normalize-high-water-mark
static double ExtractHighWaterMark(const QueuingStrategy& aStrategy,
                                   double aDefaultHWM, ErrorResult& aRv) {
  // Step 1.
  if (!aStrategy.mHighWaterMark.WasPassed()) {
    return aDefaultHWM;
  }

  // Step 2.
  double highWaterMark = aStrategy.mHighWaterMark.Value();

  // Step 3.
  if (mozilla::IsNaN(highWaterMark) || highWaterMark < 0) {
    aRv.ThrowRangeError("Invalid highWaterMark");
    return 0.0;
  }

  // Step 4.
  return highWaterMark;
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

}  // namespace dom
}  // namespace mozilla
