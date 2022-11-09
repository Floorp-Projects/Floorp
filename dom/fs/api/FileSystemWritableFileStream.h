/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
#define DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_

#include "mozilla/Logging.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/dom/FileSystemWritableFileStreamChild.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "prio.h"
#include "private/pprio.h"

class nsIGlobalObject;

namespace mozilla {
extern LazyLogModule gOPFSLog;
}

namespace mozilla::ipc {
class FileDescriptor;
}  // namespace mozilla::ipc

namespace mozilla {

class ErrorResult;

namespace dom {

class ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams;
class Promise;

namespace fs {
class FileSystemRequestHandler;
}

class FileSystemWritableFileStream final : public WritableStream {
 public:
  // No cycle-collection DECL/IMPL macros, because WritableStream calls
  // HoldJSObjects()
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
      nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
      const fs::FileSystemEntryMetadata& aMetadata,
      RefPtr<FileSystemWritableFileStreamChild>& aActor);

  FileSystemWritableFileStream(
      nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
      const fs::FileSystemEntryMetadata& aMetadata,
      RefPtr<FileSystemWritableFileStreamChild>& aActor);

  FileSystemWritableFileStream(
      nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
      const fs::FileSystemEntryMetadata& aMetadata,
      RefPtr<FileSystemWritableFileStreamChild>& aActor,
      fs::FileSystemRequestHandler* aRequestHandler);

  void ClearActor();

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  already_AddRefed<Promise> Write(
      const ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams& aData,
      ErrorResult& aError);

  bool DoSeek(RefPtr<Promise>& aPromise, uint64_t aPosition);

  already_AddRefed<Promise> Seek(uint64_t aPosition, ErrorResult& aError);

  already_AddRefed<Promise> Truncate(uint64_t aSize, ErrorResult& aError);

  already_AddRefed<Promise> Close(ErrorResult& aRv);

  bool IsClosed() const { return !mActor || !mActor->MutableFileDescPtr(); }

 protected:
  // XXX Hack, want to inherit from FileSystemHandle as well
  // nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<FileSystemManager> mManager;
  fs::FileSystemEntryMetadata mMetadata;
  const UniquePtr<fs::FileSystemRequestHandler> mRequestHandler;

 private:
  nsresult WriteBlob(Blob* aBlob, uint64_t& aWritten);
  //  nsresult WriteBlobImpl(BlobImpl* aBlobImpl, uint64_t& aWritten);

  virtual ~FileSystemWritableFileStream();

  RefPtr<FileSystemWritableFileStreamChild> mActor;
  RefPtr<TaskQueue> mTaskQueue;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
