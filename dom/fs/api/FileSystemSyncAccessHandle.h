/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_
#define DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_

#include "mozilla/Logging.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
extern LazyLogModule gOPFSLog;

class ErrorResult;

namespace ipc {
class FileDescriptor;
}  // namespace ipc

namespace dom {

class FileSystemAccessHandleChild;
struct FileSystemReadWriteOptions;
class FileSystemManager;
class MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class Promise;

namespace fs {
class FileSystemRequestHandler;
}  // namespace fs

class FileSystemSyncAccessHandle final : public nsISupports,
                                         public nsWrapperCache {
 public:
  FileSystemSyncAccessHandle(nsIGlobalObject* aGlobal,
                             RefPtr<FileSystemManager>& aManager,
                             RefPtr<FileSystemAccessHandleChild> aActor,
                             const fs::FileSystemEntryMetadata& aMetadata,
                             fs::FileSystemRequestHandler* aRequestHandler);

  FileSystemSyncAccessHandle(nsIGlobalObject* aGlobal,
                             RefPtr<FileSystemManager>& aManager,
                             RefPtr<FileSystemAccessHandleChild> aActor,
                             const fs::FileSystemEntryMetadata& aMetadata);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileSystemSyncAccessHandle)

  void ClearActor();

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

  already_AddRefed<Promise> Truncate(uint64_t aSize, ErrorResult& aError);

  already_AddRefed<Promise> GetSize(ErrorResult& aError);

  already_AddRefed<Promise> Flush(ErrorResult& aError);

  already_AddRefed<Promise> Close(ErrorResult& aError);

 private:
  virtual ~FileSystemSyncAccessHandle();

  nsCOMPtr<nsIGlobalObject> mGlobal;

  RefPtr<FileSystemManager> mManager;

  RefPtr<FileSystemAccessHandleChild> mActor;

  const fs::FileSystemEntryMetadata mMetadata;

  const UniquePtr<fs::FileSystemRequestHandler> mRequestHandler;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_
