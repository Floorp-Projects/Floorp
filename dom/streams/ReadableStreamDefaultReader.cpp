/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/RootedDictionary.h"
#include "js/PropertyAndElement.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "jsapi.h"
#include "mozilla/dom/ReadableStreamDefaultReaderBinding.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION(ReadableStreamGenericReader, mClosedPromise, mStream,
                         mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ReadableStreamGenericReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ReadableStreamGenericReader)
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ReadableStreamGenericReader)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamGenericReader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_INHERITED(ReadableStreamDefaultReader,
                                                ReadableStreamGenericReader,
                                                mReadRequests)
NS_IMPL_ADDREF_INHERITED(ReadableStreamDefaultReader,
                         ReadableStreamGenericReader)
NS_IMPL_RELEASE_INHERITED(ReadableStreamDefaultReader,
                          ReadableStreamGenericReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamDefaultReader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(ReadableStreamGenericReader)

ReadableStreamDefaultReader::ReadableStreamDefaultReader(nsISupports* aGlobal)
    : ReadableStreamGenericReader(do_QueryInterface(aGlobal)),
      nsWrapperCache() {}

ReadableStreamDefaultReader::~ReadableStreamDefaultReader() {
  mReadRequests.clear();
}

JSObject* ReadableStreamDefaultReader::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ReadableStreamDefaultReader_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#readable-stream-reader-generic-initialize
bool ReadableStreamReaderGenericInitialize(ReadableStreamGenericReader* aReader,
                                           ReadableStream* aStream,
                                           ErrorResult& aRv) {
  // Step 1.
  aReader->SetStream(aStream);

  // Step 2.
  aStream->SetReader(aReader);

  aReader->SetClosedPromise(Promise::Create(aReader->GetParentObject(), aRv));
  if (aRv.Failed()) {
    return false;
  }

  switch (aStream->State()) {
      // Step 3.
    case ReadableStream::ReaderState::Readable:
      // Step 3.1
      // Promise created above.
      return true;
    // Step 4.
    case ReadableStream::ReaderState::Closed:
      // Step 4.1.
      aReader->ClosedPromise()->MaybeResolve(JS::UndefinedHandleValue);

      return true;
    // Step 5.
    case ReadableStream::ReaderState::Errored: {
      // Step 5.1 Implicit
      // Step 5.2
      JS::RootingContext* rcx = RootingCx();
      JS::Rooted<JS::Value> rootedError(rcx, aStream->StoredError());
      aReader->ClosedPromise()->MaybeReject(rootedError);

      // Step 5.3
      aReader->ClosedPromise()->SetSettledPromiseIsHandled();
      return true;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown ReaderState");
      return false;
  }
}

// https://streams.spec.whatwg.org/#default-reader-constructor &&
// https://streams.spec.whatwg.org/#set-up-readable-stream-default-reader
/* static */
already_AddRefed<ReadableStreamDefaultReader>
ReadableStreamDefaultReader::Constructor(const GlobalObject& aGlobal,
                                         ReadableStream& aStream,
                                         ErrorResult& aRv) {
  RefPtr<ReadableStreamDefaultReader> reader =
      new ReadableStreamDefaultReader(aGlobal.GetAsSupports());

  // https://streams.spec.whatwg.org/#set-up-readable-stream-default-reader
  // Step 1.
  if (aStream.Locked()) {
    aRv.ThrowTypeError(
        "Cannot create a new reader for a readable stream already locked by "
        "another reader.");
    return nullptr;
  }

  // Step 2.
  RefPtr<ReadableStream> streamPtr = &aStream;
  if (!ReadableStreamReaderGenericInitialize(reader, streamPtr, aRv)) {
    return nullptr;
  }

  // Step 3.
  reader->mReadRequests.clear();

  return reader.forget();
}

void Read_ReadRequest::ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                                  ErrorResult& aRv) {
  // https://streams.spec.whatwg.org/#default-reader-read Step 3.
  // chunk steps, given chunk:
  //  Step 1. Resolve promise with «[ "value" → chunk, "done" → false ]».

  // Value may need to be wrapped if stream and reader are in different
  // compartments.
  JS::Rooted<JS::Value> chunk(aCx, aChunk);
  if (!JS_WrapValue(aCx, &chunk)) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  RootedDictionary<ReadableStreamReadResult> result(aCx);
  result.mValue = chunk;
  result.mDone.Construct(false);

  // Ensure that the object is created with the current global.
  JS::Rooted<JS::Value> value(aCx);
  if (!ToJSValue(aCx, std::move(result), &value)) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  mPromise->MaybeResolve(value);
}

void Read_ReadRequest::CloseSteps(JSContext* aCx, ErrorResult& aRv) {
  // https://streams.spec.whatwg.org/#default-reader-read Step 3.
  // close steps:
  //  Step 1. Resolve promise with «[ "value" → undefined, "done" → true ]».
  RootedDictionary<ReadableStreamReadResult> result(aCx);
  result.mValue.setUndefined();
  result.mDone.Construct(true);

  JS::Rooted<JS::Value> value(aCx);
  if (!ToJSValue(aCx, std::move(result), &value)) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  mPromise->MaybeResolve(value);
}

void Read_ReadRequest::ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> e,
                                  ErrorResult& aRv) {
  // https://streams.spec.whatwg.org/#default-reader-read Step 3.
  // error steps:
  //  Step 1. Reject promise with e.
  mPromise->MaybeReject(e);
}

NS_IMPL_CYCLE_COLLECTION(ReadRequest)
NS_IMPL_CYCLE_COLLECTION_INHERITED(Read_ReadRequest, ReadRequest, mPromise)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ReadRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ReadRequest)

NS_IMPL_ADDREF_INHERITED(Read_ReadRequest, ReadRequest)
NS_IMPL_RELEASE_INHERITED(Read_ReadRequest, ReadRequest)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadRequest)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Read_ReadRequest)
NS_INTERFACE_MAP_END_INHERITING(ReadRequest)

// https://streams.spec.whatwg.org/#readable-stream-default-reader-read
void ReadableStreamDefaultReaderRead(JSContext* aCx,
                                     ReadableStreamGenericReader* aReader,
                                     ReadRequest* aRequest, ErrorResult& aRv) {
  // Step 1.
  ReadableStream* stream = aReader->GetStream();

  // Step 2.
  MOZ_ASSERT(stream);

  // Step 3.
  stream->SetDisturbed(true);

  switch (stream->State()) {
    // Step 4.
    case ReadableStream::ReaderState::Closed: {
      aRequest->CloseSteps(aCx, aRv);
      return;
    }

    case ReadableStream::ReaderState::Errored: {
      JS::Rooted<JS::Value> storedError(aCx, stream->StoredError());
      aRequest->ErrorSteps(aCx, storedError, aRv);
      return;
    }

    case ReadableStream::ReaderState::Readable: {
      RefPtr<ReadableStreamController> controller(stream->Controller());
      MOZ_ASSERT(controller);
      controller->PullSteps(aCx, aRequest, aRv);
      return;
    }
  }
}

// Return a raw pointer here to avoid refcounting, but make sure it's safe
// (the object should be kept alive by the callee).
// https://streams.spec.whatwg.org/#default-reader-read
already_AddRefed<Promise> ReadableStreamDefaultReader::Read(ErrorResult& aRv) {
  // Step 1.
  if (!mStream) {
    aRv.ThrowTypeError("Reading is not possible after calling releaseLock.");
    return nullptr;
  }

  // Step 2.
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aRv);

  // Step 3.
  RefPtr<ReadRequest> request = new Read_ReadRequest(promise);

  // Step 4.
  AutoEntryScript aes(mGlobal, "ReadableStreamDefaultReader::Read");
  JSContext* cx = aes.cx();

  ReadableStreamDefaultReaderRead(cx, this, request, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 5.
  return promise.forget();
}

// https://streams.spec.whatwg.org/#readable-stream-reader-generic-release
void ReadableStreamReaderGenericRelease(ReadableStreamGenericReader* aReader,
                                        ErrorResult& aRv) {
  // Step 1. Let stream be reader.[[stream]].
  RefPtr<ReadableStream> stream = aReader->GetStream();

  // Step 2. Assert: stream is not undefined.
  MOZ_ASSERT(stream);

  // Step 3. Assert: stream.[[reader]] is reader.
  MOZ_ASSERT(stream->GetReader() == aReader);

  // Step 4. If stream.[[state]] is "readable", reject reader.[[closedPromise]]
  // with a TypeError exception.
  if (aReader->GetStream()->State() == ReadableStream::ReaderState::Readable) {
    aReader->ClosedPromise()->MaybeRejectWithTypeError(
        "Releasing lock on readable stream");
  } else {
    // Step 5. Otherwise, set reader.[[closedPromise]] to a promise rejected
    // with a TypeError exception.
    RefPtr<Promise> promise = Promise::CreateRejectedWithTypeError(
        aReader->GetParentObject(), "Lock Released"_ns, aRv);
    aReader->SetClosedPromise(promise.forget());
  }

  // Step 6. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
  aReader->ClosedPromise()->SetSettledPromiseIsHandled();

  // Step 7. Perform ! stream.[[controller]].[[ReleaseSteps]]().
  stream->Controller()->ReleaseSteps();

  // Step 8. Set stream.[[reader]] to undefined.
  stream->SetReader(nullptr);

  // Step 9. Set reader.[[stream]] to undefined.
  aReader->SetStream(nullptr);
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreadererrorreadrequests
void ReadableStreamDefaultReaderErrorReadRequests(
    JSContext* aCx, ReadableStreamDefaultReader* aReader,
    JS::Handle<JS::Value> aError, ErrorResult& aRv) {
  // Step 1. Let readRequests be reader.[[readRequests]].
  LinkedList<RefPtr<ReadRequest>> readRequests =
      std::move(aReader->ReadRequests());

  // Step 2. Set reader.[[readRequests]] to a new empty list.
  // Note: The std::move already cleared this anyway.
  aReader->ReadRequests().clear();

  // Step 3. For each readRequest of readRequests,
  while (RefPtr<ReadRequest> readRequest = readRequests.popFirst()) {
    // Step 3.1. Perform readRequest’s error steps, given e.
    readRequest->ErrorSteps(aCx, aError, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreaderrelease
void ReadableStreamDefaultReaderRelease(JSContext* aCx,
                                        ReadableStreamDefaultReader* aReader,
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

  // Step 3. Perform ! ReadableStreamDefaultReaderErrorReadRequests(reader, e).
  ReadableStreamDefaultReaderErrorReadRequests(aCx, aReader, error, aRv);
}

// https://streams.spec.whatwg.org/#default-reader-release-lock
void ReadableStreamDefaultReader::ReleaseLock(ErrorResult& aRv) {
  // Step 1. If this.[[stream]] is undefined, return.
  if (!mStream) {
    return;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    return aRv.ThrowUnknownError("Internal error");
  }
  JSContext* cx = jsapi.cx();

  // Step 2. Perform ! ReadableStreamDefaultReaderRelease(this).
  RefPtr<ReadableStreamDefaultReader> thisRefPtr = this;
  ReadableStreamDefaultReaderRelease(cx, thisRefPtr, aRv);
}

// https://streams.spec.whatwg.org/#generic-reader-closed
already_AddRefed<Promise> ReadableStreamGenericReader::Closed() const {
  // Step 1.
  return do_AddRef(mClosedPromise);
}

// https://streams.spec.whatwg.org/#readable-stream-reader-generic-cancel
MOZ_CAN_RUN_SCRIPT
static already_AddRefed<Promise> ReadableStreamGenericReaderCancel(
    JSContext* aCx, ReadableStreamGenericReader* aReader,
    JS::Handle<JS::Value> aReason, ErrorResult& aRv) {
  // Step 1 (Strong ref for below call).
  RefPtr<ReadableStream> stream = aReader->GetStream();

  // Step 2.
  MOZ_ASSERT(stream);

  // Step 3.
  return ReadableStreamCancel(aCx, stream, aReason, aRv);
}

already_AddRefed<Promise> ReadableStreamGenericReader::Cancel(
    JSContext* aCx, JS::Handle<JS::Value> aReason, ErrorResult& aRv) {
  // Step 1. If this.[[stream]] is undefined,
  // return a promise rejected with a TypeError exception.
  if (!mStream) {
    aRv.ThrowTypeError("Canceling is not possible after calling releaseLock.");
    return nullptr;
  }

  // Step 2. Return ! ReadableStreamReaderGenericCancel(this, reason).
  return ReadableStreamGenericReaderCancel(aCx, this, aReason, aRv);
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-reader
void SetUpReadableStreamDefaultReader(ReadableStreamDefaultReader* aReader,
                                      ReadableStream* aStream,
                                      ErrorResult& aRv) {
  // Step 1.
  if (IsReadableStreamLocked(aStream)) {
    return aRv.ThrowTypeError(
        "Cannot get a new reader for a readable stream already locked by "
        "another reader.");
  }

  // Step 2.
  if (!ReadableStreamReaderGenericInitialize(aReader, aStream, aRv)) {
    return;
  }

  // Step 3.
  aReader->ReadRequests().clear();
}

}  // namespace mozilla::dom
