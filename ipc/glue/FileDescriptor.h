/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_FileDescriptor_h
#define mozilla_ipc_FileDescriptor_h

#include "base/basictypes.h"
#include "base/process.h"
#include "mozilla/UniquePtrExtensions.h"

namespace mozilla {
namespace ipc {

// This class is used by IPDL to share file descriptors across processes. When
// sending a FileDescriptor, IPDL will transfer a duplicate of the handle into
// the remote process.
//
// To use this class add 'FileDescriptor' as an argument in the IPDL protocol
// and then pass a file descriptor from C++ to the Send method. The Recv method
// will receive a FileDescriptor& on which PlatformHandle() can be called to
// return the platform file handle.
class FileDescriptor {
 public:
  typedef base::ProcessId ProcessId;

  using UniquePlatformHandle = mozilla::UniqueFileHandle;
  using PlatformHandleType = UniquePlatformHandle::ElementType;

  // This should only ever be created by IPDL.
  struct IPDLPrivate {};

  // Represents an invalid handle.
  FileDescriptor();

  // Copy constructor will duplicate a new handle.
  FileDescriptor(const FileDescriptor& aOther);

  FileDescriptor(FileDescriptor&& aOther);

  // This constructor will duplicate a new handle.
  // The caller still have to close aHandle.
  explicit FileDescriptor(PlatformHandleType aHandle);

  explicit FileDescriptor(UniquePlatformHandle&& aHandle);

  ~FileDescriptor();

  FileDescriptor& operator=(const FileDescriptor& aOther);

  FileDescriptor& operator=(FileDescriptor&& aOther);

  // Tests mHandle against a well-known invalid platform-specific file handle
  // (e.g. -1 on POSIX, INVALID_HANDLE_VALUE on Windows).
  bool IsValid() const;

  // Returns a duplicated handle, it is caller's responsibility to close the
  // handle.
  UniquePlatformHandle ClonePlatformHandle() const;

  // Extracts the underlying handle and makes this object an invalid handle.
  // (Compare UniquePtr::release.)
  UniquePlatformHandle TakePlatformHandle();

  // Only used in nsTArray.
  bool operator==(const FileDescriptor& aOther) const;

 private:
  static UniqueFileHandle Clone(PlatformHandleType aHandle);

  UniquePlatformHandle mHandle;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_FileDescriptor_h
