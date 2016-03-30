/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemBase_h
#define mozilla_dom_FileSystemBase_h

#include "nsAutoPtr.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class BlobImpl;
class Directory;

class FileSystemBase
{
  NS_INLINE_DECL_REFCOUNTING(FileSystemBase)
public:

  // Create file system object from its string representation.
  static already_AddRefed<FileSystemBase>
  DeserializeDOMPath(const nsAString& aString);

  FileSystemBase();

  virtual void
  Shutdown();

  // SerializeDOMPath the FileSystem to string.
  virtual void
  SerializeDOMPath(nsAString& aOutput) const = 0;

  virtual already_AddRefed<FileSystemBase>
  Clone() = 0;

  virtual nsISupports*
  GetParentObject() const;

  /*
   * Get the virtual name of the root directory. This name will be exposed to
   * the content page.
   */
  virtual void
  GetRootName(nsAString& aRetval) const = 0;

  /*
   * Return the local root path of the FileSystem implementation.
   * For OSFileSystem, this is equal to the path of the root Directory;
   * For DeviceStorageFileSystem, this is the path of the SDCard, parent
   * directory of the exposed root Directory (per type).
   */
  const nsAString&
  LocalOrDeviceStorageRootPath() const
  {
    return mLocalOrDeviceStorageRootPath;
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

  /*
   * Get the permission name required to access this file system.
   */
  const nsCString&
  GetPermission() const
  {
    return mPermission;
  }

  bool
  RequiresPermissionChecks() const
  {
    return mRequiresPermissionChecks;
  }

  // CC methods
  virtual void Unlink() {}
  virtual void Traverse(nsCycleCollectionTraversalCallback &cb) {}

protected:
  virtual ~FileSystemBase();

  // The local path of the root (i.e. the OS path, with OS path separators, of
  // the OS directory that acts as the root of this OSFileSystem).
  // This path must be set by the FileSystem implementation immediately
  // because it will be used for the validation of any FileSystemTaskBase.
  // The concept of this path is that, any task will never go out of it and this
  // must be considered the OS 'root' of the current FileSystem. Different
  // Directory object can have different OS 'root' path.
  // To be more clear, any path managed by this FileSystem implementation must
  // be discendant of this local root path.
  // The reason why it's not just called 'localRootPath' is because for
  // DeviceStorage this contains the path of the device storage SDCard, that is
  // the parent directory of the exposed root path.
  nsString mLocalOrDeviceStorageRootPath;

  bool mShutdown;

  // The permission name required to access the file system.
  nsCString mPermission;

  bool mRequiresPermissionChecks;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemBase_h
