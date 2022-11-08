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
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/FileSystemWritableFileStreamChild.h"
#include "mozilla/dom/PFileSystemManager.h"
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

class FileSystemWritableFileStream final : public WritableStream {
 public:
  static already_AddRefed<FileSystemWritableFileStream> Create(
      nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
      RefPtr<FileSystemWritableFileStreamChild> aActor,
      const fs::FileSystemEntryMetadata& aMetadata);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemWritableFileStream,
                                           WritableStream)

  void ClearActor();

  bool IsClosed() const { return !mActor || !mActor->MutableFileDescPtr(); }

  already_AddRefed<Promise> Close(ErrorResult& aRv);

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

 protected:
  RefPtr<FileSystemManager> mManager;
  fs::FileSystemEntryMetadata mMetadata;

 private:
  FileSystemWritableFileStream(nsIGlobalObject* aGlobal,
                               RefPtr<FileSystemManager>& aManager,
                               RefPtr<FileSystemWritableFileStreamChild> aActor,
                               const fs::FileSystemEntryMetadata& aMetadata);

  virtual ~FileSystemWritableFileStream();

  nsresult WriteBlob(Blob* aBlob, uint64_t& aWritten);

  RefPtr<FileSystemWritableFileStreamChild> mActor;
  RefPtr<TaskQueue> mTaskQueue;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
