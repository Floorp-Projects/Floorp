/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DeviceStorageFileSystem_h
#define mozilla_dom_DeviceStorageFileSystem_h

#include "mozilla/dom/FileSystemBase.h"
#include "nsString.h"

class nsDOMDeviceStorage;

namespace mozilla {
namespace dom {

class DeviceStorageFileSystem
  : public FileSystemBase
{
public:
  DeviceStorageFileSystem(const nsAString& aStorageType,
                          const nsAString& aStorageName);

  void
  Init(nsDOMDeviceStorage* aDeviceStorage);

  // Overrides FileSystemBase

  virtual void
  Shutdown() override;

  virtual nsPIDOMWindow*
  GetWindow() const override;

  virtual already_AddRefed<nsIFile>
  GetLocalFile(const nsAString& aRealPath) const override;

  virtual bool
  GetRealPath(FileImpl* aFile, nsAString& aRealPath) const override;

  virtual const nsAString&
  GetRootName() const override;

  virtual bool
  IsSafeFile(nsIFile* aFile) const override;

  virtual bool
  IsSafeDirectory(Directory* aDir) const override;
private:
  virtual
  ~DeviceStorageFileSystem();

  bool
  LocalPathToRealPath(const nsAString& aLocalPath, nsAString& aRealPath) const;

  nsString mStorageType;
  nsString mStorageName;

  // The local path of the root. Only available in the parent process.
  // In the child process, we don't use it and its value should be empty.
  nsString mLocalRootPath;
  nsString mNormalizedLocalRootPath;
  nsDOMDeviceStorage* mDeviceStorage;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DeviceStorageFileSystem_h
