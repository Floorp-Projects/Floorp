/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMHANDLE_H_
#define DOM_FS_FILESYSTEMHANDLE_H_

#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/Logging.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

extern LazyLogModule gOPFSLog;

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

class ErrorResult;

namespace dom {

class DOMString;
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
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FileSystemHandle)

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

 protected:
  virtual ~FileSystemHandle() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;

  RefPtr<FileSystemManager> mManager;

  const fs::FileSystemEntryMetadata mMetadata;

  const UniquePtr<fs::FileSystemRequestHandler> mRequestHandler;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMHANDLE_H_
