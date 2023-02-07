/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_
#define DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_

#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;
class TaskQueue;

namespace dom {

class FileSystemAccessHandleChild;
class FileSystemAccessHandleControlChild;
struct FileSystemReadWriteOptions;
class FileSystemManager;
class MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class Promise;
class StrongWorkerRef;

class FileSystemSyncAccessHandle final : public nsISupports,
                                         public nsWrapperCache {
 public:
  enum struct State : uint8_t { Initial = 0, Open, Closing, Closed };

  static Result<RefPtr<FileSystemSyncAccessHandle>, nsresult> Create(
      nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
      mozilla::ipc::RandomAccessStreamParams&& aStreamParams,
      mozilla::ipc::ManagedEndpoint<PFileSystemAccessHandleChild>&&
          aAccessHandleChildEndpoint,
      mozilla::ipc::Endpoint<PFileSystemAccessHandleControlChild>&&
          aAccessHandleControlChildEndpoint,
      const fs::FileSystemEntryMetadata& aMetadata);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileSystemSyncAccessHandle)

  void LastRelease();

  void ClearActor();

  void ClearControlActor();

  bool IsOpen() const;

  bool IsClosing() const;

  bool IsClosed() const;

  [[nodiscard]] RefPtr<BoolPromise> BeginClose();

  [[nodiscard]] RefPtr<BoolPromise> OnClose();

  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  uint64_t Read(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
      const FileSystemReadWriteOptions& aOptions, ErrorResult& aRv);

  uint64_t Write(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
      const FileSystemReadWriteOptions& aOptions, ErrorResult& aRv);

  void Truncate(uint64_t aSize, ErrorResult& aError);

  uint64_t GetSize(ErrorResult& aError);

  void Flush(ErrorResult& aError);

  void Close();

 private:
  FileSystemSyncAccessHandle(
      nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
      mozilla::ipc::RandomAccessStreamParams&& aStreamParams,
      RefPtr<FileSystemAccessHandleChild> aActor,
      RefPtr<FileSystemAccessHandleControlChild> aControlActor,
      RefPtr<TaskQueue> aIOTaskQueue,
      const fs::FileSystemEntryMetadata& aMetadata);

  virtual ~FileSystemSyncAccessHandle();

  uint64_t ReadOrWrite(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
      const FileSystemReadWriteOptions& aOptions, const bool aRead,
      ErrorResult& aRv);

  nsresult EnsureStream();

  nsCOMPtr<nsIGlobalObject> mGlobal;

  RefPtr<FileSystemManager> mManager;

  RefPtr<FileSystemAccessHandleChild> mActor;

  RefPtr<FileSystemAccessHandleControlChild> mControlActor;

  RefPtr<TaskQueue> mIOTaskQueue;

  nsCOMPtr<nsIRandomAccessStream> mStream;

  RefPtr<StrongWorkerRef> mWorkerRef;

  MozPromiseHolder<BoolPromise> mClosePromiseHolder;

  mozilla::ipc::RandomAccessStreamParams mStreamParams;

  const fs::FileSystemEntryMetadata mMetadata;

  State mState;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_
