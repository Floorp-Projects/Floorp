/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ArrayBuffer.h"
#include "js/experimental/TypedData.h"
#include "mozilla/dom/ReadableStreamBYOBReader.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamBYOBReaderBinding.h"
#include "mozilla/dom/ReadableStreamGenericReader.h"
#include "mozilla/dom/ReadIntoRequest.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"

// Temporary Includes
#include "mozilla/dom/ReadableByteStreamController.h"
#include "mozilla/dom/ReadableStreamBYOBRequest.h"
#include "mozilla/dom/ReadableStreamBYOBReader.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ReadableStreamBYOBReader,
                                   ReadableStreamGenericReader,
                                   mReadIntoRequests)
NS_IMPL_ADDREF_INHERITED(ReadableStreamBYOBReader, ReadableStreamGenericReader)
NS_IMPL_RELEASE_INHERITED(ReadableStreamBYOBReader, ReadableStreamGenericReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamBYOBReader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(ReadableStreamGenericReader)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ReadableStreamBYOBReader,
                                               ReadableStreamGenericReader)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

JSObject* ReadableStreamBYOBReader::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ReadableStreamBYOBReader_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-byob-reader
void SetUpReadableStreamBYOBReader(JSContext* aCx,
                                   ReadableStreamBYOBReader* reader,
                                   ReadableStream& stream, ErrorResult& rv) {
  // Step 1. If !IsReadableStreamLocked(stream) is true, throw a TypeError
  // exception.
  if (IsReadableStreamLocked(&stream)) {
    rv.ThrowTypeError("Trying to read locked stream");
    return;
  }

  // Step 2. If stream.[[controller]] does not implement
  // ReadableByteStreamController, throw a TypeError exception.
  if (!stream.Controller()->IsByte()) {
    rv.ThrowTypeError("Trying to read with incompatible controller");
    return;
  }

  // Step 3. Perform ! ReadableStreamReaderGenericInitialize(reader, stream).
  ReadableStreamReaderGenericInitialize(aCx, reader, &stream, rv);
  if (rv.Failed()) {
    return;
  }

  // Step 4. Set reader.[[readIntoRequests]] to a new empty list.
  reader->ReadIntoRequests().clear();
}

// https://streams.spec.whatwg.org/#byob-reader-constructor
/* static */ already_AddRefed<ReadableStreamBYOBReader>
ReadableStreamBYOBReader::Constructor(const GlobalObject& global,
                                      ReadableStream& stream, ErrorResult& rv) {
  nsCOMPtr<nsIGlobalObject> globalObject =
      do_QueryInterface(global.GetAsSupports());
  RefPtr<ReadableStreamBYOBReader> reader =
      new ReadableStreamBYOBReader(globalObject);

  // Step 1.
  SetUpReadableStreamBYOBReader(global.Context(), reader, stream, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  return reader.forget();
}

struct Read_ReadIntoRequest final : public ReadIntoRequest {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Read_ReadIntoRequest,
                                           ReadIntoRequest)

  RefPtr<Promise> mPromise;

  explicit Read_ReadIntoRequest(Promise* aPromise) : mPromise(aPromise) {}

  void ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                  ErrorResult& errorResult) override {
    MOZ_ASSERT(aChunk.isObject());
    // https://streams.spec.whatwg.org/#byob-reader-read Step 6.
    //
    // chunk steps, given chunk:
    //         Resolve promise with «[ "value" → chunk, "done" → false ]».

    // We need to wrap this as the chunk could have come from
    // another compartment.
    JS::RootedObject chunk(aCx, &aChunk.toObject());
    if (!JS_WrapObject(aCx, &chunk)) {
      return;
    }

    ReadableStreamBYOBReadResult result;
    result.mValue.Construct();
    result.mValue.Value().Init(chunk);
    result.mDone.Construct(false);

    mPromise->MaybeResolve(result);
  }

  void CloseSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                  ErrorResult& errorResult) override {
    MOZ_ASSERT(aChunk.isObject());
    // https://streams.spec.whatwg.org/#byob-reader-read Step 6.
    //
    // close steps, given chunk:
    // Resolve promise with «[ "value" → chunk, "done" → true ]».

    // We need to wrap this as the chunk could have come from
    // another compartment.
    JS::RootedObject chunk(aCx, &aChunk.toObject());
    if (!JS_WrapObject(aCx, &chunk)) {
      return;
    }

    ReadableStreamBYOBReadResult result;
    result.mValue.Construct();
    result.mValue.Value().Init(chunk);
    result.mDone.Construct(true);

    mPromise->MaybeResolve(result);
  }

  void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> e,
                  ErrorResult& errorResult) override {
    // https://streams.spec.whatwg.org/#byob-reader-read Step 6.
    //
    // error steps, given e:
    // Reject promise with e.
    mPromise->MaybeReject(e);
  }

 protected:
  virtual ~Read_ReadIntoRequest() = default;
};

NS_IMPL_CYCLE_COLLECTION(ReadIntoRequest)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ReadIntoRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ReadIntoRequest)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadIntoRequest)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_INHERITED(Read_ReadIntoRequest, ReadIntoRequest,
                                   mPromise)
NS_IMPL_ADDREF_INHERITED(Read_ReadIntoRequest, ReadIntoRequest)
NS_IMPL_RELEASE_INHERITED(Read_ReadIntoRequest, ReadIntoRequest)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Read_ReadIntoRequest)
NS_INTERFACE_MAP_END_INHERITING(ReadIntoRequest)

// https://streams.spec.whatwg.org/#readable-stream-byob-reader-read
void ReadableStreamBYOBReaderRead(JSContext* aCx,
                                  ReadableStreamBYOBReader* aReader,
                                  JS::HandleObject aView,
                                  ReadIntoRequest* aReadIntoRequest,
                                  ErrorResult& aRv) {
  // Step 1.Let stream be reader.[[stream]].
  ReadableStream* stream = aReader->GetStream();

  // Step 2. Assert: stream is not undefined.
  MOZ_ASSERT(stream);

  // Step 3. Set stream.[[disturbed]] to true.
  stream->SetDisturbed(true);

  // Step 4. If stream.[[state]] is "errored", perform readIntoRequest’s error
  // steps given stream.[[storedError]].
  if (stream->State() == ReadableStream::ReaderState::Errored) {
    JS::RootedValue error(aCx, stream->StoredError());

    aReadIntoRequest->ErrorSteps(aCx, error, aRv);
    return;
  }

  // Step 5. Otherwise, perform
  // !ReadableByteStreamControllerPullInto(stream.[[controller]], view,
  // readIntoRequest).
  MOZ_ASSERT(stream->Controller()->IsByte());
  RefPtr<ReadableByteStreamController> controller(
      stream->Controller()->AsByte());
  ReadableByteStreamControllerPullInto(aCx, controller, aView, aReadIntoRequest,
                                       aRv);
}

// https://streams.spec.whatwg.org/#byob-reader-read
already_AddRefed<Promise> ReadableStreamBYOBReader::Read(
    const ArrayBufferView& aArray, ErrorResult& aRv) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetParentObject())) {
    // MG:XXX: Do I need to have reported this error to aRv?
    return nullptr;
  }
  JSContext* cx = jsapi.cx();

  JS::RootedObject view(cx, aArray.Obj());

  // Step 1. If view.[[ByteLength]] is 0, return a promise rejected with a
  // TypeError exception.
  if (JS_GetArrayBufferViewByteLength(view) == 0) {
    // Binding code should convert this thrown value into a rejected promise.
    aRv.ThrowTypeError("Zero Length View");
    return nullptr;
  }

  // Step 2. If view.[[ViewedArrayBuffer]].[[ArrayBufferByteLength]] is 0,
  // return a promise rejected with a TypeError exception.
  bool isSharedMemory;
  JS::RootedObject viewedArrayBuffer(
      cx, JS_GetArrayBufferViewBuffer(cx, view, &isSharedMemory));
  if (!viewedArrayBuffer) {
    aRv.StealExceptionFromJSContext(cx);
    return nullptr;
  }

  if (JS::GetArrayBufferByteLength(viewedArrayBuffer) == 0) {
    aRv.ThrowTypeError("zero length viewed buffer");
    return nullptr;
  }

  // Step 3. If ! IsDetachedBuffer(view.[[ViewedArrayBuffer]]) is true, return a
  // promise rejected with a TypeError exception.
  if (JS::IsDetachedArrayBufferObject(viewedArrayBuffer)) {
    aRv.ThrowTypeError("Detatched Buffer");
    return nullptr;
  }

  // Step 4. If this.[[stream]] is undefined, return a promise rejected with a
  // TypeError exception.
  if (!GetStream()) {
    aRv.ThrowTypeError("Reader has undefined stream");
    return nullptr;
  }

  // Step 5.
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 6. Let readIntoRequest be a new read-into request with the following
  // items:
  RefPtr<ReadIntoRequest> readIntoRequest = new Read_ReadIntoRequest(promise);

  // Step 7. Perform ! ReadableStreamBYOBReaderRead(this, view,
  // readIntoRequest).
  ReadableStreamBYOBReaderRead(cx, this, view, readIntoRequest, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 8. Return promise.
  return promise.forget();
}

// https://streams.spec.whatwg.org/#byob-reader-release-lock
void ReadableStreamBYOBReader::ReleaseLock(ErrorResult& aRv) {
  if (!GetStream()) {
    return;
  }

  if (!ReadIntoRequests().isEmpty()) {
    aRv.ThrowTypeError("ReadIntoRequests not empty");
    return;
  }

  ReadableStreamReaderGenericRelease(this, aRv);
}

// https://streams.spec.whatwg.org/#acquire-readable-stream-byob-reader
already_AddRefed<ReadableStreamBYOBReader> AcquireReadableStreamBYOBReader(
    JSContext* aCx, ReadableStream* aStream, ErrorResult& aRv) {
  // Step 1. Let reader be a new ReadableStreamBYOBReader.
  RefPtr<ReadableStreamBYOBReader> reader =
      new ReadableStreamBYOBReader(aStream->GetParentObject());

  // Step 2. Perform ? SetUpReadableStreamBYOBReader(reader, stream).
  SetUpReadableStreamBYOBReader(aCx, reader, *aStream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 3. Return reader.
  return reader.forget();
}

}  // namespace dom
}  // namespace mozilla
