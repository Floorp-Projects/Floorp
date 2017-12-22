/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_FileDescriptor_h
#define mozilla_ipc_FileDescriptor_h

#include "base/basictypes.h"
#include "base/process.h"
#include "mozilla/UniquePtr.h"

#ifdef XP_WIN
// Need the HANDLE typedef.
#include <winnt.h>
#include <cstdint>
#else
#include "base/file_descriptor_posix.h"
#endif

namespace mozilla {
namespace ipc {

// This class is used by IPDL to share file descriptors across processes. When
// sending a FileDescriptor IPDL will first duplicate a platform-specific file
// handle type ('PlatformHandleType') into a handle that is valid in the other
// process. Then IPDL will convert the duplicated handle into a type suitable
// for pickling ('PickleType') and then send that through the IPC pipe. In the
// receiving process the pickled data is converted into a platform-specific file
// handle and then returned to the receiver.
//
// To use this class add 'FileDescriptor' as an argument in the IPDL protocol
// and then pass a file descriptor from C++ to the Call/Send method. The
// Answer/Recv method will receive a FileDescriptor& on which PlatformHandle()
// can be called to return the platform file handle.
class FileDescriptor
{
public:
  typedef base::ProcessId ProcessId;

#ifdef XP_WIN
  typedef HANDLE PlatformHandleType;
  typedef HANDLE PickleType;
#else
  typedef int PlatformHandleType;
  typedef base::FileDescriptor PickleType;
#endif

  struct PlatformHandleHelper
  {
    MOZ_IMPLICIT PlatformHandleHelper(PlatformHandleType aHandle);
    MOZ_IMPLICIT PlatformHandleHelper(std::nullptr_t);
    bool operator != (std::nullptr_t) const;
    operator PlatformHandleType () const;
#ifdef XP_WIN
    operator std::intptr_t () const;
#endif
  private:
    PlatformHandleType mHandle;
  };
  struct PlatformHandleDeleter
  {
    typedef PlatformHandleHelper pointer;
    void operator () (PlatformHandleHelper aHelper);
  };
  typedef UniquePtr<PlatformHandleType, PlatformHandleDeleter> UniquePlatformHandle;

  // This should only ever be created by IPDL.
  struct IPDLPrivate
  {};

  // Represents an invalid handle.
  FileDescriptor();

  // Copy constructor will duplicate a new handle.
  FileDescriptor(const FileDescriptor& aOther);

  FileDescriptor(FileDescriptor&& aOther);

  // This constructor will duplicate a new handle.
  // The caller still have to close aHandle.
  explicit FileDescriptor(PlatformHandleType aHandle);

  // This constructor WILL NOT duplicate the handle.
  // FileDescriptor takes the ownership from IPC message.
  FileDescriptor(const IPDLPrivate&, const PickleType& aPickle);

  ~FileDescriptor();

  FileDescriptor&
  operator=(const FileDescriptor& aOther);

  FileDescriptor&
  operator=(FileDescriptor&& aOther);

  // Performs platform-specific actions to duplicate mHandle in the other
  // process (e.g. dup() on POSIX, DuplicateHandle() on Windows). Returns a
  // pickled value that can be passed to the other process via IPC.
  PickleType
  ShareTo(const IPDLPrivate&, ProcessId aTargetPid) const;

  // Tests mHandle against a well-known invalid platform-specific file handle
  // (e.g. -1 on POSIX, INVALID_HANDLE_VALUE on Windows).
  bool
  IsValid() const;

  // Returns a duplicated handle, it is caller's responsibility to close the
  // handle.
  UniquePlatformHandle
  ClonePlatformHandle() const;

  // Only used in nsTArray.
  bool
  operator==(const FileDescriptor& aOther) const;

private:
  friend struct PlatformHandleTrait;

  void
  Assign(const FileDescriptor& aOther);

  void
  Close();

  static bool
  IsValid(PlatformHandleType aHandle);

  static PlatformHandleType
  Clone(PlatformHandleType aHandle);

  static void
  Close(PlatformHandleType aHandle);

  PlatformHandleType mHandle;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_FileDescriptor_h
