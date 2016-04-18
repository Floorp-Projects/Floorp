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

  virtual already_AddRefed<FileSystemBase>
  Clone() override;

  virtual bool
  ShouldCreateDirectory() override { return true; }

  virtual void
  Shutdown() override;

  virtual nsISupports*
  GetParentObject() const override;

  virtual void
  GetDirectoryName(nsIFile* aFile, nsAString& aRetval,
                   ErrorResult& aRv) const override;

  virtual bool
  IsSafeFile(nsIFile* aFile) const override;

  virtual bool
  IsSafeDirectory(Directory* aDir) const override;

  virtual void
  SerializeDOMPath(nsAString& aSerializedString) const override;

  virtual bool
  NeedToGoToMainThread() const override { return true; }

  virtual nsresult
  MainThreadWork() override;

private:
  virtual
  ~DeviceStorageFileSystem();

  nsString mStorageType;
  nsString mStorageName;

  uint64_t mWindowId;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DeviceStorageFileSystem_h
