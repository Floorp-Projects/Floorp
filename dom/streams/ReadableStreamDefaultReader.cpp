/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/dom/ReadableStream.h"
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

namespace mozilla {
namespace dom {

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
bool ReadableStreamReaderGenericInitialize(JSContext* aCx,
                                           ReadableStreamGenericReader* aReader,
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
    case ReadableStream::ReaderState::Errored:
      // Step 5.1 Implicit
      // Step 5.2
      JS::RootedValue rootedError(aCx, aStream->StoredError());
      aReader->ClosedPromise()->MaybeReject(rootedError);

      // Step 5.3
      aReader->ClosedPromise()->SetSettledPromiseIsHandled();
      return true;
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
  if (!ReadableStreamReaderGenericInitialize(aGlobal.Context(), reader,
                                             streamPtr, aRv)) {
    return nullptr;
  }

  // Step 3.
  reader->mReadRequests.clear();

  return reader.forget();
}

static bool CreateValueDonePair(JSContext* aCx, bool forAuthorCode,
                                JS::HandleValue aValue, bool aDone,
                                JS::MutableHandleValue aReturnValue) {
  JS::RootedObject obj(
      aCx, forAuthorCode ? JS_NewPlainObject(aCx)
                         : JS_NewObjectWithGivenProto(aCx, nullptr, nullptr));
  if (!obj) {
    return false;
  }

  // Value may need to be wrapped if stream and reader are in different
  // compartments.
  JS::RootedValue value(aCx, aValue);
  if (!JS_WrapValue(aCx, &value)) {
    return false;
  }

  if (!JS_DefineProperty(aCx, obj, "value", value, JSPROP_ENUMERATE)) {
    return false;
  }
  JS::RootedValue done(aCx, JS::BooleanValue(aDone));
  if (!JS_DefineProperty(aCx, obj, "done", done, JSPROP_ENUMERATE)) {
    return false;
  }

  aReturnValue.setObject(*obj);
  return true;
}

void Read_ReadRequest::ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                                  ErrorResult& aRv) {
  // Step 1.
  JS::RootedValue resolvedValue(aCx);
  if (!CreateValueDonePair(aCx, mForAuthorCode, aChunk, false,
                           &resolvedValue)) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  mPromise->MaybeResolve(resolvedValue);
}

void Read_ReadRequest::CloseSteps(JSContext* aCx, ErrorResult& aRv) {
  // Step 1.
  JS::RootedValue undefined(aCx, JS::UndefinedValue());
  JS::RootedValue resolvedValue(aCx);
  if (!CreateValueDonePair(aCx, mForAuthorCode, undefined, true,
                           &resolvedValue)) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  mPromise->MaybeResolve(resolvedValue);
}

void Read_ReadRequest::ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> e,
                                  ErrorResult& aRv) {
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
      JS::RootedValue storedError(aCx, stream->StoredError());
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
already_AddRefed<Promise> ReadableStreamDefaultReader::Read(JSContext* aCx,
                                                            ErrorResult& aRv) {
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
  ReadableStreamDefaultReaderRead(aCx, this, request, aRv);
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
    RefPtr<Promise> promise = Promise::Create(aReader->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    promise->MaybeRejectWithTypeError("Lock Released");
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
void SetUpReadableStreamDefaultReader(JSContext* aCx,
                                      ReadableStreamDefaultReader* aReader,
                                      ReadableStream* aStream,
                                      ErrorResult& aRv) {
  // Step 1.
  if (IsReadableStreamLocked(aStream)) {
    return aRv.ThrowTypeError(
        "Cannot get a new reader for a readable stream already locked by "
        "another reader.");
  }

  // Step 2.
  if (!ReadableStreamReaderGenericInitialize(aCx, aReader, aStream, aRv)) {
    return;
  }

  // Step 3.
  aReader->ReadRequests().clear();
}

}  // namespace dom
}  // namespace mozilla
