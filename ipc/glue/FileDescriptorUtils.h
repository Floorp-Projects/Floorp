
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_FileDescriptorUtils_h
#define mozilla_ipc_FileDescriptorUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "nsIRunnable.h"

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
  CloseFileRunnable(const FileDescriptor& aFileDescriptor)
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

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_FileDescriptorUtils_h
