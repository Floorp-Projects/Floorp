/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_FileDescriptor_h
#define mozilla_ipc_FileDescriptor_h

#include "base/basictypes.h"
#include "base/process.h"
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

  FileDescriptor(PlatformHandleType aHandle)
  : mHandle(aHandle)
  { }

  FileDescriptor(const IPDLPrivate&, const PickleType& aPickle)
#ifdef XP_WIN
  : mHandle(aPickle)
#else
  : mHandle(aPickle.fd)
#endif
  { }

  // Performs platform-specific actions to duplicate mHandle in the other
  // process (e.g. dup() on POSIX, DuplicateHandle() on Windows). Returns a
  // pickled value that can be passed to the other process via IPC.
  PickleType
  ShareTo(const IPDLPrivate&, ProcessHandle aOtherProcess) const;

  // Tests mHandle against a well-known invalid platform-specific file handle
  // (e.g. -1 on POSIX, INVALID_HANDLE_VALUE on Windows).
  bool
  IsValid() const;

  PlatformHandleType
  PlatformHandle() const
  {
    return mHandle;
  }

  bool
  operator==(const FileDescriptor& aOther) const
  {
    return mHandle == aOther.mHandle;
  }

private:
  PlatformHandleType mHandle;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_FileDescriptor_h
