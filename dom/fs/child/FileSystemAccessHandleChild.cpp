/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemAccessHandleChild.h"

#include "mozilla/dom/FileSystemSyncAccessHandle.h"
#include "private/pprio.h"

namespace mozilla::dom {

FileSystemAccessHandleChild::FileSystemAccessHandleChild(
    const ::mozilla::ipc::FileDescriptor& aFileDescriptor)
    : mAccessHandle(nullptr) {
  auto rawFD = aFileDescriptor.ClonePlatformHandle();
  mFileDesc = PR_ImportFile(PROsfd(rawFD.release()));
}

FileSystemAccessHandleChild::~FileSystemAccessHandleChild() = default;

void FileSystemAccessHandleChild::SetAccessHandle(
    FileSystemSyncAccessHandle* aAccessHandle) {
  MOZ_ASSERT(aAccessHandle);
  MOZ_ASSERT(!mAccessHandle);

  mAccessHandle = aAccessHandle;
}

PRFileDesc* FileSystemAccessHandleChild::MutableFileDescPtr() const {
  MOZ_ASSERT(mFileDesc);

  return mFileDesc;
}

void FileSystemAccessHandleChild::Close() {
  MOZ_ASSERT(mFileDesc);

  PR_Close(mFileDesc);
  mFileDesc = nullptr;

  PFileSystemAccessHandleChild::Send__delete__(this);
}

void FileSystemAccessHandleChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (mFileDesc) {
    PR_Close(mFileDesc);
    mFileDesc = nullptr;
  }

  if (mAccessHandle) {
    mAccessHandle->ClearActor();
    mAccessHandle = nullptr;
  }
}

}  // namespace mozilla::dom
