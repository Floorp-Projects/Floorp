/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMHANDLE_H_
#define DOM_FS_FILESYSTEMHANDLE_H_

#include "mozilla/Logging.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla {
extern LazyLogModule gOPFSLog;
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

class nsIGlobalObject;

namespace mozilla {

extern LazyLogModule gOPFSLog;

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

class ErrorResult;

namespace dom {

class FileSystemDirectoryHandle;
class FileSystemFileHandle;
enum class FileSystemHandleKind : uint8_t;
class FileSystemManager;
class FileSystemManagerChild;
class Promise;

namespace fs {
class FileSystemRequestHandler;
}  // namespace fs

class FileSystemHandle : public nsISupports, public nsWrapperCache {
 public:
  FileSystemHandle(nsIGlobalObject* aGlobal,
                   RefPtr<FileSystemManager>& aManager,
                   const fs::FileSystemEntryMetadata& aMetadata,
                   fs::FileSystemRequestHandler* aRequestHandler);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileSystemHandle)

  const fs::EntryId& GetId() const { return mMetadata.entryId(); }

  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  virtual FileSystemHandleKind Kind() const = 0;

  void GetName(nsAString& aResult);

  already_AddRefed<Promise> IsSameEntry(FileSystemHandle& aOther,
                                        ErrorResult& aError) const;

  // [Serializable] implementation
  static already_AddRefed<FileSystemHandle> ReadStructuredClone(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);

  virtual bool WriteStructuredClone(JSContext* aCx,
                                    JSStructuredCloneWriter* aWriter) const;

  already_AddRefed<Promise> Move(const nsAString& aName, ErrorResult& aError);

  already_AddRefed<Promise> Move(FileSystemDirectoryHandle& aParent,
                                 ErrorResult& aError);

  already_AddRefed<Promise> Move(FileSystemDirectoryHandle& aParent,
                                 const nsAString& aName, ErrorResult& aError);

  already_AddRefed<Promise> Move(const fs::EntryId& aParentId,
                                 const nsAString& aName, ErrorResult& aError);

  void UpdateMetadata(const fs::FileSystemEntryMetadata& aMetadata) {
    mMetadata = aMetadata;
  }

 protected:
  virtual ~FileSystemHandle() = default;

  static already_AddRefed<FileSystemFileHandle> ConstructFileHandle(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);

  static already_AddRefed<FileSystemDirectoryHandle> ConstructDirectoryHandle(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);

  nsCOMPtr<nsIGlobalObject> mGlobal;

  RefPtr<FileSystemManager> mManager;

  // move() can change names/directories
  fs::FileSystemEntryMetadata mMetadata;

  const UniquePtr<fs::FileSystemRequestHandler> mRequestHandler;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMHANDLE_H_
