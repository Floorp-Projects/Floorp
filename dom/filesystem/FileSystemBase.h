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

  // The decision about doing or not doing the permission check cannot be done
  // everywhere because, for some FileSystemBase implementation, this depends on
  // a preference.
  // This enum describes all the possible decisions. The implementation will do
  // the check on the main-thread in the child and in the parent process when
  // needed.
  // Note: the permission check should not fail in PBackground because that
  // means that the child has been compromised. If this happens the child
  // process is killed.
  enum ePermissionCheckType {
    // When on the main-thread, we must check if we have
    // device.storage.prompt.testing set to true.
    ePermissionCheckByTestingPref,

    // No permission check must be done.
    ePermissionCheckNotRequired,

    // Permission check is required.
    ePermissionCheckRequired,

    // This is the default value. We crash if this is let like this.
    eNotSet
  };

  ePermissionCheckType
  PermissionCheckType() const
  {
    MOZ_ASSERT(mPermissionCheckType != eNotSet);
    return mPermissionCheckType;
  }

  // IPC initialization
  // See how these 2 methods are used in FileSystemTaskChildBase.

  virtual bool
  NeedToGoToMainThread() const { return false; }

  virtual nsresult
  MainThreadWork() { return NS_ERROR_FAILURE; }

  virtual bool
  ClonableToDifferentThreadOrProcess() const { return false; }

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
  // The reason why it's not just called 'localRootPath' is because for
  // DeviceStorage this contains the path of the device storage SDCard, that is
  // the parent directory of the exposed root path.
  nsString mLocalOrDeviceStorageRootPath;

  bool mShutdown;

  // The permission name required to access the file system.
  nsCString mPermission;

  ePermissionCheckType mPermissionCheckType;

#ifdef DEBUG
  PRThread* mOwningThread;
#endif
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemBase_h
