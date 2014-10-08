/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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

class Directory;
class FileImpl;

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

  /*
   * Create nsIFile object with the given real path (absolute DOM path).
   */
  virtual already_AddRefed<nsIFile>
  GetLocalFile(const nsAString& aRealPath) const = 0;

  /*
   * Get the virtual name of the root directory. This name will be exposed to
   * the content page.
   */
  virtual const nsAString&
  GetRootName() const = 0;

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
  virtual bool
  GetRealPath(FileImpl* aFile, nsAString& aRealPath) const = 0;

  /*
   * Get the permission name required to access this file system.
   */
  const nsCString&
  GetPermission() const
  {
    return mPermission;
  }

  bool
  IsTesting() const
  {
    return mIsTesting;
  }
protected:
  virtual ~FileSystemBase();

  // The string representation of the file system.
  nsString mString;

  bool mShutdown;

  // The permission name required to access the file system.
  nsCString mPermission;

  bool mIsTesting;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemBase_h
