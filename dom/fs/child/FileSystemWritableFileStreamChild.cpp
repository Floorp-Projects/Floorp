/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemWritableFileStreamChild.h"

#include "mozilla/dom/FileSystemWritableFileStream.h"
#include "private/pprio.h"

namespace mozilla {
extern LazyLogModule gOPFSLog;
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

namespace mozilla::dom {

FileSystemWritableFileStreamChild::FileSystemWritableFileStreamChild(
    const ::mozilla::ipc::FileDescriptor& aFileDescriptor)
    : mStream(nullptr) {
  auto rawFD = aFileDescriptor.ClonePlatformHandle();
  mFileDesc = PR_ImportFile(PROsfd(rawFD.release()));
  LOG(("Created new WritableFileStreamChild %p", this));
}

FileSystemWritableFileStreamChild::~FileSystemWritableFileStreamChild() =
    default;

void FileSystemWritableFileStreamChild::SetStream(
    FileSystemWritableFileStream* aStream) {
  mStream = aStream;
}

PRFileDesc* FileSystemWritableFileStreamChild::MutableFileDescPtr() const {
  MOZ_ASSERT(mFileDesc);

  return mFileDesc;
}

void FileSystemWritableFileStreamChild::Close() {
  MOZ_ASSERT(mFileDesc);

  LOG(("Closing WritableFileStreamChild %p", this));
  PR_Close(mFileDesc);
  mFileDesc = nullptr;

  PFileSystemWritableFileStreamChild::Send__delete__(this);
}

void FileSystemWritableFileStreamChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("Destroy WritableFileStreamChild %p", this));
  if (mFileDesc) {
    PR_Close(mFileDesc);
    mFileDesc = nullptr;
  }

  if (mStream) {
    mStream->ClearActor();
    mStream = nullptr;
  }
}

}  // namespace mozilla::dom
