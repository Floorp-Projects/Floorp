/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemDirectoryReader_h
#define mozilla_dom_FileSystemDirectoryReader_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/FileSystemDirectoryEntry.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class Directory;
class FileSystem;
class FileSystemEntriesCallback;

class FileSystemDirectoryReader : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileSystemDirectoryReader)

  explicit FileSystemDirectoryReader(FileSystemDirectoryEntry* aDirectoryEntry,
                                     FileSystem* aFileSystem,
                                     Directory* aDirectory);

  nsIGlobalObject* GetParentObject() const {
    return mParentEntry->GetParentObject();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual void ReadEntries(
      FileSystemEntriesCallback& aSuccessCallback,
      const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
      ErrorResult& aRv);

 protected:
  virtual ~FileSystemDirectoryReader();

 private:
  RefPtr<FileSystemDirectoryEntry> mParentEntry;
  RefPtr<FileSystem> mFileSystem;
  RefPtr<Directory> mDirectory;

  bool mAlreadyRead;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FileSystemDirectoryReader_h
