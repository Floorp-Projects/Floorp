/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_FileDescriptor_h
#define mozilla_ipc_FileDescriptor_h

#include "base/basictypes.h"
#include "base/process.h"
#include "mozilla/DebugOnly.h"
#include "nscore.h"

#ifdef XP_WIN
// Need the HANDLE typedef.
#include <winnt.h>
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
  typedef base::ProcessHandle ProcessHandle;

#ifdef XP_WIN
  typedef HANDLE PlatformHandleType;
  typedef HANDLE PickleType;
#else
  typedef int PlatformHandleType;
  typedef base::FileDescriptor PickleType;
#endif

  // This should only ever be created by IPDL.
  struct IPDLPrivate
  {};

  FileDescriptor();

  FileDescriptor(const FileDescriptor& aOther)
    : mHandleCreatedByOtherProcess(false),
      mHandleCreatedByOtherProcessWasUsed(false)
  {
    // Don't use operator= here because that will call
    // CloseCurrentProcessHandle() on this (uninitialized) object.
    Assign(aOther);
  }

  FileDescriptor(PlatformHandleType aHandle);

  FileDescriptor(const IPDLPrivate&, const PickleType& aPickle)
#ifdef XP_WIN
  : mHandle(aPickle)
#else
  : mHandle(aPickle.fd)
#endif
  , mHandleCreatedByOtherProcess(true)
  , mHandleCreatedByOtherProcessWasUsed(false)
  { }

  ~FileDescriptor()
  {
    CloseCurrentProcessHandle();
  }

  FileDescriptor&
  operator=(const FileDescriptor& aOther)
  {
    CloseCurrentProcessHandle();
    Assign(aOther);
    return *this;
  }

  // Performs platform-specific actions to duplicate mHandle in the other
  // process (e.g. dup() on POSIX, DuplicateHandle() on Windows). Returns a
  // pickled value that can be passed to the other process via IPC.
  PickleType
  ShareTo(const IPDLPrivate&, ProcessHandle aOtherProcess) const;

  // Tests mHandle against a well-known invalid platform-specific file handle
  // (e.g. -1 on POSIX, INVALID_HANDLE_VALUE on Windows).
  bool
  IsValid() const
  {
    return IsValid(mHandle);
  }

  PlatformHandleType
  PlatformHandle() const
  {
    if (mHandleCreatedByOtherProcess) {
      mHandleCreatedByOtherProcessWasUsed = true;
    }
    return mHandle;
  }

  bool
  operator==(const FileDescriptor& aOther) const
  {
    return mHandle == aOther.mHandle;
  }

private:
  void
  Assign(const FileDescriptor& aOther)
  {
    if (aOther.mHandleCreatedByOtherProcess) {
      mHandleCreatedByOtherProcess = true;
      mHandleCreatedByOtherProcessWasUsed =
        aOther.mHandleCreatedByOtherProcessWasUsed;
      mHandle = aOther.PlatformHandle();
    } else {
      DuplicateInCurrentProcess(aOther.PlatformHandle());
      mHandleCreatedByOtherProcess = false;
      mHandleCreatedByOtherProcessWasUsed = false;
    }
  }

  static bool
  IsValid(PlatformHandleType aHandle);

  void
  DuplicateInCurrentProcess(PlatformHandleType aHandle);

  void
  CloseCurrentProcessHandle();

  PlatformHandleType mHandle;

  // If this is true then this instance is created by IPDL to ferry a handle to
  // its eventual consumer and we never close the handle. If this is false then
  // we are a RAII wrapper around the handle and we close the handle on
  // destruction.
  bool mHandleCreatedByOtherProcess;

  // This is to ensure that we don't leak the handle (which is only possible
  // when we're in the receiving process).
  mutable DebugOnly<bool> mHandleCreatedByOtherProcessWasUsed;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_FileDescriptor_h
