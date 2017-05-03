/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemBase_h
#define mozilla_dom_FileSystemBase_h

#include "nsString.h"
#include "Directory.h"

namespace mozilla {
namespace dom {

class BlobImpl;

class FileSystemBase
{
public:
  NS_INLINE_DECL_REFCOUNTING(FileSystemBase)

  FileSystemBase();

  virtual void
  Shutdown();

  // SerializeDOMPath the FileSystem to string.
  virtual void
  SerializeDOMPath(nsAString& aOutput) const = 0;

  virtual already_AddRefed<FileSystemBase>
  Clone() = 0;

  virtual bool
  ShouldCreateDirectory() = 0;

  virtual nsISupports*
  GetParentObject() const;

  virtual void
  GetDirectoryName(nsIFile* aFile, nsAString& aRetval,
                   ErrorResult& aRv) const;

  void
  GetDOMPath(nsIFile* aFile, nsAString& aRetval, ErrorResult& aRv) const;

  /*
   * Return the local root path of the FileSystem implementation.
   * For OSFileSystem, this is equal to the path of the root Directory;
   * For DeviceStorageFileSystem, this is the path of the SDCard, parent
   * directory of the exposed root Directory (per type).
   */
  const nsAString&
  LocalRootPath() const
  {
    return mLocalRootPath;
  }

  bool
  IsShutdown() const
  {
    return mShutdown;
  }

  virtual bool
  IsSafeFile(nsIFile* aFile) const;

  virtual bool
  IsSafeDirectory(Directory* aDir) const;

  bool
  GetRealPath(BlobImpl* aFile, nsIFile** aPath) const;

  // CC methods
  virtual void Unlink() {}
  virtual void Traverse(nsCycleCollectionTraversalCallback &cb) {}

  void
  AssertIsOnOwningThread() const;

protected:
  virtual ~FileSystemBase();

  // The local path of the root (i.e. the OS path, with OS path separators, of
  // the OS directory that acts as the root of this OSFileSystem).
  // This path must be set by the FileSystem implementation immediately
  // because it will be used for the validation of any FileSystemTaskChildBase.
  // The concept of this path is that, any task will never go out of it and this
  // must be considered the OS 'root' of the current FileSystem. Different
  // Directory object can have different OS 'root' path.
  // To be more clear, any path managed by this FileSystem implementation must
  // be discendant of this local root path.
  nsString mLocalRootPath;

  bool mShutdown;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemBase_h
