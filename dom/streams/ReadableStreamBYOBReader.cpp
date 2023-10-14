/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReadableStreamBYOBReader.h"

#include "ReadIntoRequest.h"
#include "js/ArrayBuffer.h"
#include "js/experimental/TypedData.h"
#include "mozilla/dom/ReadableStreamBYOBReader.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamBYOBReaderBinding.h"
#include "mozilla/dom/ReadableStreamGenericReader.h"
#include "mozilla/dom/RootedDictionary.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"

// Temporary Includes
#include "mozilla/dom/ReadableByteStreamController.h"
#include "mozilla/dom/ReadableStreamBYOBRequest.h"

namespace mozilla::dom {

using namespace streams_abstract;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_INHERITED(ReadableStreamBYOBReader,
                                                ReadableStreamGenericReader,
                                                mReadIntoRequests)
NS_IMPL_ADDREF_INHERITED(ReadableStreamBYOBReader, ReadableStreamGenericReader)
NS_IMPL_RELEASE_INHERITED(ReadableStreamBYOBReader, ReadableStreamGenericReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamBYOBReader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(ReadableStreamGenericReader)

ReadableStreamBYOBReader::ReadableStreamBYOBReader(nsISupports* aGlobal)
    : ReadableStreamGenericReader(do_QueryInterface(aGlobal)) {}

JSObject* ReadableStreamBYOBReader::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ReadableStreamBYOBReader_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-byob-reader
void SetUpReadableStreamBYOBReader(ReadableStreamBYOBReader* reader,
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
  ReadableStreamReaderGenericInitialize(reader, &stream);

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
  SetUpReadableStreamBYOBReader(reader, stream, rv);
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
                  ErrorResult& aRv) override {
    MOZ_ASSERT(aChunk.isObject());
    // https://streams.spec.whatwg.org/#byob-reader-read Step 6.
    //
    // chunk steps, given chunk:
    //         Resolve promise with «[ "value" → chunk, "done" → false ]».

    // We need to wrap this as the chunk could have come from
    // another compartment.
    JS::Rooted<JSObject*> chunk(aCx, &aChunk.toObject());
    if (!JS_WrapObject(aCx, &chunk)) {
      aRv.StealExceptionFromJSContext(aCx);
      return;
    }

    RootedDictionary<ReadableStreamReadResult> result(aCx);
    result.mValue = aChunk;
    result.mDone.Construct(false);

    mPromise->MaybeResolve(result);
  }

  void CloseSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                  ErrorResult& aRv) override {
    MOZ_ASSERT(aChunk.isObject() || aChunk.isUndefined());
    // https://streams.spec.whatwg.org/#byob-reader-read Step 6.
    //
    // close steps, given chunk:
    // Resolve promise with «[ "value" → chunk, "done" → true ]».
    RootedDictionary<ReadableStreamReadResult> result(aCx);
    if (aChunk.isObject()) {
      // We need to wrap this as the chunk could have come from
      // another compartment.
      JS::Rooted<JSObject*> chunk(aCx, &aChunk.toObject());
      if (!JS_WrapObject(aCx, &chunk)) {
        aRv.StealExceptionFromJSContext(aCx);
        return;
      }

      result.mValue = aChunk;
    }
    result.mDone.Construct(true);

    mPromise->MaybeResolve(result);
  }

  void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> e,
                  ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#byob-reader-read Step 6.
    //
    // error steps, given e:
    // Reject promise with e.
    mPromise->MaybeReject(e);
  }

 protected:
  ~Read_ReadIntoRequest() override = default;
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

namespace streams_abstract {
// https://streams.spec.whatwg.org/#readable-stream-byob-reader-read
void ReadableStreamBYOBReaderRead(JSContext* aCx,
                                  ReadableStreamBYOBReader* aReader,
                                  JS::Handle<JSObject*> aView,
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
    JS::Rooted<JS::Value> error(aCx, stream->StoredError());

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
}  // namespace streams_abstract

// https://streams.spec.whatwg.org/#byob-reader-read
already_AddRefed<Promise> ReadableStreamBYOBReader::Read(
    const ArrayBufferView& aArray, ErrorResult& aRv) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetParentObject())) {
    aRv.ThrowUnknownError("Internal error");
    return nullptr;
  }
  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> view(cx, aArray.Obj());

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
  JS::Rooted<JSObject*> viewedArrayBuffer(
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
    aRv.ThrowTypeError("Detached Buffer");
    return nullptr;
  }

  // Step 4. If this.[[stream]] is undefined, return a promise rejected with a
  // TypeError exception.
  if (!GetStream()) {
    aRv.ThrowTypeError("Reader has undefined stream");
    return nullptr;
  }

  // Step 5.
  RefPtr<Promise> promise = Promise::CreateInfallible(GetParentObject());

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

namespace streams_abstract {

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreambyobreadererrorreadintorequests
void ReadableStreamBYOBReaderErrorReadIntoRequests(
    JSContext* aCx, ReadableStreamBYOBReader* aReader,
    JS::Handle<JS::Value> aError, ErrorResult& aRv) {
  // Step 1. Let readIntoRequests be reader.[[readIntoRequests]].
  LinkedList<RefPtr<ReadIntoRequest>> readIntoRequests =
      std::move(aReader->ReadIntoRequests());

  // Step 2. Set reader.[[readIntoRequests]] to a new empty list.
  // Note: The std::move already cleared this anyway.
  aReader->ReadIntoRequests().clear();

  // Step 3. For each readIntoRequest of readIntoRequests,
  while (RefPtr<ReadIntoRequest> readIntoRequest =
             readIntoRequests.popFirst()) {
    // Step 3.1. Perform readIntoRequest’s error steps, given e.
    readIntoRequest->ErrorSteps(aCx, aError, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreambyobreaderrelease
void ReadableStreamBYOBReaderRelease(JSContext* aCx,
                                     ReadableStreamBYOBReader* aReader,
                                     ErrorResult& aRv) {
  // Step 1. Perform ! ReadableStreamReaderGenericRelease(reader).
  ReadableStreamReaderGenericRelease(aReader, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 2. Let e be a new TypeError exception.
  ErrorResult rv;
  rv.ThrowTypeError("Releasing lock");
  JS::Rooted<JS::Value> error(aCx);
  MOZ_ALWAYS_TRUE(ToJSValue(aCx, std::move(rv), &error));

  // Step 3. Perform ! ReadableStreamBYOBReaderErrorReadIntoRequests(reader, e).
  ReadableStreamBYOBReaderErrorReadIntoRequests(aCx, aReader, error, aRv);
}

}  // namespace streams_abstract

// https://streams.spec.whatwg.org/#byob-reader-release-lock
void ReadableStreamBYOBReader::ReleaseLock(ErrorResult& aRv) {
  // Step 1. If this.[[stream]] is undefined, return.
  if (!mStream) {
    return;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    return aRv.ThrowUnknownError("Internal error");
  }
  JSContext* cx = jsapi.cx();

  // Step 2. Perform ! ReadableStreamBYOBReaderRelease(this).
  RefPtr<ReadableStreamBYOBReader> thisRefPtr = this;
  ReadableStreamBYOBReaderRelease(cx, thisRefPtr, aRv);
}

namespace streams_abstract {
// https://streams.spec.whatwg.org/#acquire-readable-stream-byob-reader
already_AddRefed<ReadableStreamBYOBReader> AcquireReadableStreamBYOBReader(
    ReadableStream* aStream, ErrorResult& aRv) {
  // Step 1. Let reader be a new ReadableStreamBYOBReader.
  RefPtr<ReadableStreamBYOBReader> reader =
      new ReadableStreamBYOBReader(aStream->GetParentObject());

  // Step 2. Perform ? SetUpReadableStreamBYOBReader(reader, stream).
  SetUpReadableStreamBYOBReader(reader, *aStream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 3. Return reader.
  return reader.forget();
}
}  // namespace streams_abstract

}  // namespace mozilla::dom
