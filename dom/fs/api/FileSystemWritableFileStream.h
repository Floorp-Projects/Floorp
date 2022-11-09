/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
#define DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_

#include "mozilla/dom/WritableStream.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams;

class FileSystemWritableFileStream final : public WritableStream {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemWritableFileStream,
                                           WritableStream)

  class StreamAlgorithms final : public UnderlyingSinkAlgorithmsBase {
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(StreamAlgorithms,
                                             UnderlyingSinkAlgorithmsBase)

    explicit StreamAlgorithms(FileSystemWritableFileStream& aStream)
        : mStream(&aStream) {}

    // Streams algorithms
    void StartCallback(JSContext* aCx,
                       WritableStreamDefaultController& aController,
                       JS::MutableHandle<JS::Value> aRetVal,
                       ErrorResult& aRv) override {
      // https://streams.spec.whatwg.org/#writablestream-set-up
      // Step 1. Let startAlgorithm be an algorithm that returns undefined.
      aRetVal.setUndefined();
    }

    MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> WriteCallback(
        JSContext* aCx, JS::Handle<JS::Value> aChunk,
        WritableStreamDefaultController& aController,
        ErrorResult& aRv) override;

    MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> AbortCallback(
        JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
        ErrorResult& aRv) override {
      // https://streams.spec.whatwg.org/#writablestream-set-up
      // Step 3.3. Return a promise resolved with undefined.
      // (No abort algorithm is defined for this interface)
      return Promise::CreateResolvedWithUndefined(mStream->GetParentObject(),
                                                  aRv);
    }

    MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> CloseCallback(
        JSContext* aCx, ErrorResult& aRv) override {
      return mStream->Close(aRv);
    };

   private:
    ~StreamAlgorithms() = default;

    RefPtr<FileSystemWritableFileStream> mStream;
  };

  static already_AddRefed<FileSystemWritableFileStream> MaybeCreate(
      nsIGlobalObject* aGlobal);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  already_AddRefed<Promise> Write(
      const ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams& aData,
      ErrorResult& aError);

  already_AddRefed<Promise> Seek(uint64_t aPosition, ErrorResult& aError);

  already_AddRefed<Promise> Truncate(uint64_t aSize, ErrorResult& aError);

  already_AddRefed<Promise> Close(ErrorResult& aRv);

 private:
  explicit FileSystemWritableFileStream(nsIGlobalObject* aGlobal)
      : WritableStream(aGlobal) {}

  ~FileSystemWritableFileStream() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
