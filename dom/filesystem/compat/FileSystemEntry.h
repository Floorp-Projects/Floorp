/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemEntry_h
#define mozilla_dom_FileSystemEntry_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/FileSystemBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class FileSystem;
class OwningFileOrDirectory;

class FileSystemEntry
  : public nsISupports
  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FileSystemEntry)

  static already_AddRefed<FileSystemEntry>
  Create(nsIGlobalObject* aGlobalObject,
         const OwningFileOrDirectory& aFileOrDirectory,
         FileSystem* aFileSystem);

  nsIGlobalObject*
  GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual bool
  IsFile() const
  {
    return false;
  }

  virtual bool
  IsDirectory() const
  {
    return false;
  }

  virtual void
  GetName(nsAString& aName, ErrorResult& aRv) const = 0;

  virtual void
  GetFullPath(nsAString& aFullPath, ErrorResult& aRv) const = 0;

  void
  GetParent(const Optional<OwningNonNull<FileSystemEntryCallback>>& aSuccessCallback,
            const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback);

  FileSystem*
  Filesystem() const
  {
    return mFileSystem;
  }

protected:
  FileSystemEntry(nsIGlobalObject* aGlobalObject,
                  FileSystemEntry* aParentEntry,
                  FileSystem* aFileSystem);
  virtual ~FileSystemEntry();

private:
  nsCOMPtr<nsIGlobalObject> mParent;
  RefPtr<FileSystemEntry> mParentEntry;
  RefPtr<FileSystem> mFileSystem;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemEntry_h
