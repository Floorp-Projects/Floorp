/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_FileDescriptorUtils_h
#define mozilla_ipc_FileDescriptorUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "nsThreadUtils.h"
#include <stdio.h>

namespace mozilla {
namespace ipc {

// When Dispatch() is called (from main thread) this class arranges to close the
// provided FileDescriptor on one of the socket transport service threads (to
// avoid main thread I/O).
class CloseFileRunnable final : public Runnable {
  typedef mozilla::ipc::FileDescriptor FileDescriptor;

  FileDescriptor mFileDescriptor;

 public:
  explicit CloseFileRunnable(const FileDescriptor& aFileDescriptor);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE

  void Dispatch();

 private:
  ~CloseFileRunnable();

  void CloseFile();
};

// On failure, FileDescriptorToFILE returns nullptr; on success,
// returns duplicated FILE*.
// This is meant for use with FileDescriptors received over IPC.
FILE* FileDescriptorToFILE(const FileDescriptor& aDesc, const char* aOpenMode);

// FILEToFileDescriptor does not consume the given FILE*; it must be
// fclose()d as normal, and this does not invalidate the returned
// FileDescriptor.
FileDescriptor FILEToFileDescriptor(FILE* aStream);

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_FileDescriptorUtils_h
