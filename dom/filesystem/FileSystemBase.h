/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemBase_h
#define mozilla_dom_FileSystemBase_h

#include "nsAutoPtr.h"
#include "nsString.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class BlobImpl;
class Directory;

class FileSystemBase
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileSystemBase)
public:

  // Create file system object from its string representation.
  static already_AddRefed<FileSystemBase>
  FromString(const nsAString& aString);

  FileSystemBase();

  virtual void
  Shutdown();

  // Get the string representation of the file system.
  const nsString&
  ToString() const
  {
    return mString;
  }

  virtual nsPIDOMWindow*
  GetWindow() const;

  /**
   * Create nsIFile object from the given real path (absolute DOM path).
   */
  already_AddRefed<nsIFile>
  GetLocalFile(const nsAString& aRealPath) const;

  /*
   * Get the virtual name of the root directory. This name will be exposed to
   * the content page.
   */
  virtual void
  GetRootName(nsAString& aRetval) const = 0;

  bool
  IsShutdown() const
  {
    return mShutdown;
  }

  virtual bool
  IsSafeFile(nsIFile* aFile) const;

  virtual bool
  IsSafeDirectory(Directory* aDir) const;

  /*
   * Get the real path (absolute DOM path) of the DOM file in the file system.
   * If succeeded, returns true. Otherwise, returns false and set aRealPath to
   * empty string.
   */
  bool
  GetRealPath(BlobImpl* aFile, nsAString& aRealPath) const;

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
protected:
  virtual ~FileSystemBase();

  bool
  LocalPathToRealPath(const nsAString& aLocalPath, nsAString& aRealPath) const;

  // The local path of the root (i.e. the OS path, with OS path separators, of
  // the OS directory that acts as the root of this OSFileSystem).
  // Only available in the parent process.
  // In the child process, we don't use it and its value should be empty.
  nsString mLocalRootPath;

  // The same, but with path separators normalized to "/".
  nsString mNormalizedLocalRootPath;

  // The string representation of the file system.
  nsString mString;

  bool mShutdown;

  // The permission name required to access the file system.
  nsCString mPermission;

  bool mRequiresPermissionChecks;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemBase_h
