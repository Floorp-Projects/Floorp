
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_FileDescriptorUtils_h
#define mozilla_ipc_FileDescriptorUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "nsIRunnable.h"
#include <stdio.h>

namespace mozilla {
namespace ipc {

// When Dispatch() is called (from main thread) this class arranges to close the
// provided FileDescriptor on one of the socket transport service threads (to
// avoid main thread I/O).
class CloseFileRunnable MOZ_FINAL : public nsIRunnable
{
  typedef mozilla::ipc::FileDescriptor FileDescriptor;

  FileDescriptor mFileDescriptor;

public:
  explicit CloseFileRunnable(const FileDescriptor& aFileDescriptor)
#ifdef DEBUG
  ;
#else
  : mFileDescriptor(aFileDescriptor)
  { }
#endif

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  void Dispatch();

private:
  ~CloseFileRunnable();

  void CloseFile();
};

// On failure, FileDescriptorToFILE closes the given descriptor; on
// success, fclose()ing the returned FILE* will close the handle.
// This is meant for use with FileDescriptors received over IPC.
FILE* FileDescriptorToFILE(const FileDescriptor& aDesc,
                           const char* aOpenMode);

// FILEToFileDescriptor does not consume the given FILE*; it must be
// fclose()d as normal, and this does not invalidate the returned
// FileDescriptor.
FileDescriptor FILEToFileDescriptor(FILE* aStream);

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_FileDescriptorUtils_h
