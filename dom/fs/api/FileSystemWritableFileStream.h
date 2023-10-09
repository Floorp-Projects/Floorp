/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
#define DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/quota/ForwardDecls.h"

class nsIGlobalObject;
class nsIRandomAccessStream;

namespace mozilla {

template <typename T>
class Buffer;
class ErrorResult;
class TaskQueue;

namespace ipc {
class RandomAccessStreamParams;
}

namespace dom {

class ArrayBufferViewOrArrayBufferOrBlobOrUTF8StringOrWriteParams;
class Blob;
class FileSystemManager;
class FileSystemWritableFileStreamChild;
class OwningArrayBufferViewOrArrayBufferOrBlobOrUSVString;
class Promise;
class StrongWorkerRef;

namespace fs {
class FileSystemThreadSafeStreamOwner;
}

class FileSystemWritableFileStream final : public WritableStream {
 public:
  using WriteDataPromise =
      MozPromise<Maybe<int64_t>, CopyableErrorResult, /* IsExclusive */ true>;

  static Result<RefPtr<FileSystemWritableFileStream>, nsresult> Create(
      const nsCOMPtr<nsIGlobalObject>& aGlobal,
      RefPtr<FileSystemManager>& aManager,
      mozilla::ipc::RandomAccessStreamParams&& aStreamParams,
      RefPtr<FileSystemWritableFileStreamChild> aActor,
      const fs::FileSystemEntryMetadata& aMetadata);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemWritableFileStream,
                                           WritableStream)

  void LastRelease() override;

  void ClearActor();

  class Command;
  RefPtr<Command> CreateCommand();

  bool IsCommandActive() const;

  bool IsOpen() const;

  bool IsFinishing() const;

  bool IsDone() const;

  [[nodiscard]] RefPtr<BoolPromise> BeginAbort();

  [[nodiscard]] RefPtr<BoolPromise> BeginClose();

  [[nodiscard]] RefPtr<BoolPromise> OnDone();

  already_AddRefed<Promise> Write(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                                  ErrorResult& aError);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Write(
      const ArrayBufferViewOrArrayBufferOrBlobOrUTF8StringOrWriteParams& aData,
      ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Seek(uint64_t aPosition,
                                                    ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Truncate(uint64_t aSize,
                                                        ErrorResult& aError);

 private:
  class CloseHandler;

  FileSystemWritableFileStream(
      const nsCOMPtr<nsIGlobalObject>& aGlobal,
      RefPtr<FileSystemManager>& aManager,
      mozilla::ipc::RandomAccessStreamParams&& aStreamParams,
      RefPtr<FileSystemWritableFileStreamChild> aActor,
      already_AddRefed<TaskQueue> aTaskQueue,
      const fs::FileSystemEntryMetadata& aMetadata);

  virtual ~FileSystemWritableFileStream();

  [[nodiscard]] RefPtr<BoolPromise> BeginFinishing(bool aShouldAbort);

  RefPtr<WriteDataPromise> Write(
      ArrayBufferViewOrArrayBufferOrBlobOrUTF8StringOrWriteParams& aData);

  template <typename T>
  RefPtr<Int64Promise> Write(const T& aData, const Maybe<uint64_t> aPosition);

  RefPtr<Int64Promise> WriteImpl(nsCOMPtr<nsIInputStream> aInputStream,
                                 const Maybe<uint64_t> aPosition);

  RefPtr<BoolPromise> Seek(uint64_t aPosition);

  RefPtr<BoolPromise> Truncate(uint64_t aSize);

  nsresult EnsureStream();

  void NoteFinishedCommand();

  [[nodiscard]] RefPtr<BoolPromise> Finish();

  RefPtr<FileSystemManager> mManager;

  RefPtr<FileSystemWritableFileStreamChild> mActor;

  RefPtr<TaskQueue> mTaskQueue;

  RefPtr<fs::FileSystemThreadSafeStreamOwner> mStreamOwner;

  RefPtr<StrongWorkerRef> mWorkerRef;

  mozilla::ipc::RandomAccessStreamParams mStreamParams;

  fs::FileSystemEntryMetadata mMetadata;

  RefPtr<CloseHandler> mCloseHandler;

  MozPromiseHolder<BoolPromise> mFinishPromiseHolder;

  bool mCommandActive;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
