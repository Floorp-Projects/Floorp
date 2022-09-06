/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_
#define DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

struct FileSystemReadWriteOptions;
class MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class Promise;

class FileSystemSyncAccessHandle final : public nsISupports,
                                         public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileSystemSyncAccessHandle)

  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  uint64_t Read(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
      const FileSystemReadWriteOptions& aOptions);

  uint64_t Write(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
      const FileSystemReadWriteOptions& aOptions);

  already_AddRefed<Promise> Truncate(uint64_t aSize, ErrorResult& aError);

  already_AddRefed<Promise> GetSize(ErrorResult& aError);

  already_AddRefed<Promise> Flush(ErrorResult& aError);

  already_AddRefed<Promise> Close(ErrorResult& aError);

 private:
  virtual ~FileSystemSyncAccessHandle() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMSYNCACCESSHANDLE_H_
