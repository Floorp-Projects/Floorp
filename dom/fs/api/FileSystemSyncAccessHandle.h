/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_
#define DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_

#include "mozilla/dom/PFileSystemManager.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class FileSystemAccessHandleChild;
struct FileSystemReadWriteOptions;
class FileSystemManager;
class MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class Promise;
class StrongWorkerRef;

class FileSystemSyncAccessHandle final : public nsISupports,
                                         public nsWrapperCache {
 public:
  static Result<RefPtr<FileSystemSyncAccessHandle>, nsresult> Create(
      nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
      RefPtr<FileSystemAccessHandleChild> aActor,
      nsCOMPtr<nsIRandomAccessStream> aStream,
      const fs::FileSystemEntryMetadata& aMetadata);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileSystemSyncAccessHandle)

  void LastRelease();

  void ClearActor();

  void CloseInternal();

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
  FileSystemSyncAccessHandle(nsIGlobalObject* aGlobal,
                             RefPtr<FileSystemManager>& aManager,
                             RefPtr<FileSystemAccessHandleChild> aActor,
                             nsCOMPtr<nsIRandomAccessStream> aStream,
                             const fs::FileSystemEntryMetadata& aMetadata);

  virtual ~FileSystemSyncAccessHandle();

  uint64_t ReadOrWrite(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
      const FileSystemReadWriteOptions& aOptions, const bool aRead,
      ErrorResult& aRv);

  nsCOMPtr<nsIGlobalObject> mGlobal;

  RefPtr<FileSystemManager> mManager;

  RefPtr<FileSystemAccessHandleChild> mActor;

  nsCOMPtr<nsIRandomAccessStream> mStream;

  RefPtr<StrongWorkerRef> mWorkerRef;

  const fs::FileSystemEntryMetadata mMetadata;

  bool mClosed;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_
