/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemWritableFileStream.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemWritableFileStreamBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/UnionConversions.h"
#include "mozilla/dom/WritableStreamDefaultController.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(FileSystemWritableFileStream,
                                               WritableStream)
NS_IMPL_CYCLE_COLLECTION_INHERITED(FileSystemWritableFileStream, WritableStream)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(
    FileSystemWritableFileStream::StreamAlgorithms,
    UnderlyingSinkAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTION_INHERITED(
    FileSystemWritableFileStream::StreamAlgorithms,
    UnderlyingSinkAlgorithmsBase, mStream)

// XXX: This is copied from the bindings layer. We need to autogenerate a
// helper for this. See bug 1784266.
static void TryGetAsUnion(
    JSContext* aCx,
    ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams& aOutput,
    JS::Handle<JS::Value> aChunk, ErrorResult& aRv) {
  JS::Rooted<JS::Value> chunk(aCx, aChunk);
  ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParamsArgument holder(
      aOutput);
  bool done = false, failed = false, tryNext;
  if (chunk.isObject()) {
    done =
        (failed =
             !holder.TrySetToArrayBufferView(aCx, &chunk, tryNext, false)) ||
        !tryNext ||
        (failed = !holder.TrySetToArrayBuffer(aCx, &chunk, tryNext, false)) ||
        !tryNext ||
        (failed = !holder.TrySetToBlob(aCx, &chunk, tryNext, false)) ||
        !tryNext;
  }
  if (!done) {
    done =
        (failed = !holder.TrySetToWriteParams(aCx, &chunk, tryNext, false)) ||
        !tryNext;
  }
  if (!done) {
    do {
      done = (failed = !holder.TrySetToUSVString(aCx, &chunk, tryNext)) ||
             !tryNext;
      break;
    } while (false);
  }
  if (failed) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }
  if (!done) {
    aRv.ThrowTypeError(
        "The chunk must be one of ArrayBufferView, ArrayBuffer, Blob, "
        "WriteParams.");
    return;
  }
}

already_AddRefed<Promise>
FileSystemWritableFileStream::StreamAlgorithms::WriteCallback(
    JSContext* aCx, JS::Handle<JS::Value> aChunk,
    WritableStreamDefaultController& aController, ErrorResult& aRv) {
  // https://fs.spec.whatwg.org/#create-a-new-filesystemwritablefilestream
  // Step 3. Let writeAlgorithm be an algorithm which takes a chunk argument ...
  ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams chunkUnion;
  TryGetAsUnion(aCx, chunkUnion, aChunk, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  // Step 3. ... and returns the result of running the write a chunk algorithm
  // with stream and chunk.
  return mStream->Write(chunkUnion, aRv);
}

// https://streams.spec.whatwg.org/#writablestream-set-up
// * This is fallible because of OOM handling of JSAPI. See bug 1762233.
// * Consider extracting this as UnderlyingSinkAlgorithmsWrapper if more classes
//   start subclassing WritableStream.
//   For now this skips step 2 - 4 as they are not required here.
// XXX(krosylight): _BOUNDARY because SetUpWritableStreamDefaultController here
// can't run script because StartCallback here is no-op. Can we let the static
// check automatically detect this situation?
/* static */
MOZ_CAN_RUN_SCRIPT_BOUNDARY already_AddRefed<FileSystemWritableFileStream>
FileSystemWritableFileStream::MaybeCreate(nsIGlobalObject* aGlobal) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(aGlobal)) {
    return nullptr;
  }
  JSContext* cx = jsapi.cx();

  // Step 5. Perform ! InitializeWritableStream(stream).
  // (Done by the constructor)
  RefPtr<FileSystemWritableFileStream> stream =
      new FileSystemWritableFileStream(aGlobal);

  // Step 1 - 3
  auto algorithms = MakeRefPtr<StreamAlgorithms>(*stream);

  // Step 6. Let controller be a new WritableStreamDefaultController.
  auto controller =
      MakeRefPtr<WritableStreamDefaultController>(aGlobal, *stream);

  // Step 7. Perform ! SetUpWritableStreamDefaultController(stream, controller,
  // startAlgorithm, writeAlgorithm, closeAlgorithmWrapper,
  // abortAlgorithmWrapper, highWaterMark, sizeAlgorithm).
  IgnoredErrorResult rv;
  SetUpWritableStreamDefaultController(
      cx, stream, controller, algorithms,
      // default highWaterMark
      1,
      // default sizeAlgorithm
      // (nullptr returns 1, See WritableStream::Constructor for details)
      nullptr, rv);
  if (rv.Failed()) {
    return nullptr;
  }
  return stream.forget();
}

// WebIDL Boilerplate

JSObject* FileSystemWritableFileStream::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return FileSystemWritableFileStream_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

already_AddRefed<Promise> FileSystemWritableFileStream::Write(
    const ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams& aData,
    ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemWritableFileStream::Seek(
    uint64_t aPosition, ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemWritableFileStream::Truncate(
    uint64_t aSize, ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemWritableFileStream::Close(
    ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

}  // namespace mozilla::dom
