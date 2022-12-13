/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
#define DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_

#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/WritableStream.h"

class nsIGlobalObject;

namespace mozilla {

template <typename T>
class Buffer;
class ErrorResult;

namespace dom {

class ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams;
class Blob;
class FileSystemManager;
class FileSystemWritableFileStreamChild;
class Promise;

class FileSystemWritableFileStream final : public WritableStream {
 public:
  static already_AddRefed<FileSystemWritableFileStream> Create(
      nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
      RefPtr<FileSystemWritableFileStreamChild> aActor,
      const ::mozilla::ipc::FileDescriptor& aFileDescriptor,
      const fs::FileSystemEntryMetadata& aMetadata);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemWritableFileStream,
                                           WritableStream)

  void LastRelease() override;

  void ClearActor();

  bool IsClosed() const { return mClosed; }

  void Close();

  already_AddRefed<Promise> Write(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                                  ErrorResult& aError);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Write(
      const ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams& aData,
      ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Seek(uint64_t aPosition,
                                                    ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Truncate(uint64_t aSize,
                                                        ErrorResult& aError);

 private:
  FileSystemWritableFileStream(
      nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
      RefPtr<FileSystemWritableFileStreamChild> aActor,
      const ::mozilla::ipc::FileDescriptor& aFileDescriptor,
      const fs::FileSystemEntryMetadata& aMetadata);

  virtual ~FileSystemWritableFileStream();

  template <typename T>
  void Write(const T& aData, const Maybe<uint64_t> aPosition,
             RefPtr<Promise> aPromise);

  void Seek(uint64_t aPosition, RefPtr<Promise> aPromise);

  void Truncate(uint64_t aSize, RefPtr<Promise> aPromise);

  Result<uint64_t, nsresult> WriteBuffer(Buffer<char>&& aBuffer,
                                         const Maybe<uint64_t> aPosition);

  Result<uint64_t, nsresult> WriteStream(nsCOMPtr<nsIInputStream> aStream,
                                         const Maybe<uint64_t> aPosition);

  Result<Ok, nsresult> SeekPosition(uint64_t aPosition);

  RefPtr<FileSystemManager> mManager;

  RefPtr<FileSystemWritableFileStreamChild> mActor;

  PRFileDesc* mFileDesc;

  fs::FileSystemEntryMetadata mMetadata;

  bool mClosed;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
