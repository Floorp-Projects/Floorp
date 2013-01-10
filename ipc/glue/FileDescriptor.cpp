/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileDescriptor.h"

#include "mozilla/Assertions.h"
#include "nsDebug.h"

#ifdef XP_WIN
#include <windows.h>
#define INVALID_HANDLE INVALID_HANDLE_VALUE
#else
#include <unistd.h>
#define INVALID_HANDLE -1
#endif

using mozilla::ipc::FileDescriptor;

FileDescriptor::FileDescriptor()
: mHandle(INVALID_HANDLE)
{ }

FileDescriptor::PickleType
FileDescriptor::ShareTo(const FileDescriptor::IPDLPrivate&,
                        FileDescriptor::ProcessHandle aOtherProcess) const
{
#ifdef XP_WIN
  if (mHandle == INVALID_HANDLE) {
    return INVALID_HANDLE;
  }

  PlatformHandleType newHandle;
  if (!DuplicateHandle(GetCurrentProcess(), mHandle, aOtherProcess, &newHandle,
                       0, FALSE, DUPLICATE_SAME_ACCESS)) {
    NS_WARNING("Failed to duplicate file handle!");
    return INVALID_HANDLE;
  }

  return newHandle;
#else // XP_WIN
  if (mHandle == INVALID_HANDLE) {
    return base::FileDescriptor();
  }

  PlatformHandleType newHandle = dup(mHandle);
  if (newHandle < 0) {
    NS_WARNING("Failed to duplicate file descriptor!");
    return base::FileDescriptor();
  }

  // This file descriptor must be closed once the caller is done using it, so
  // pass true here for the 'auto_close' argument.
  return base::FileDescriptor(newHandle, true);
#endif

  MOZ_NOT_REACHED("Must not get here!");
  return PickleType();
}

bool
FileDescriptor::IsValid() const
{
  return mHandle != INVALID_HANDLE;
}
