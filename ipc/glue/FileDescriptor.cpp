/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileDescriptor.h"

#include "mozilla/Assertions.h"
#include "nsDebug.h"

#ifdef XP_WIN

#include <windows.h>
#define INVALID_HANDLE INVALID_HANDLE_VALUE

#else // XP_WIN

#include <unistd.h>

#ifndef OS_POSIX
#define OS_POSIX
#endif

#include "base/eintr_wrapper.h"
#define INVALID_HANDLE -1

#endif // XP_WIN

using mozilla::ipc::FileDescriptor;

FileDescriptor::FileDescriptor()
: mHandle(INVALID_HANDLE), mHandleCreatedByOtherProcess(false),
  mHandleCreatedByOtherProcessWasUsed(false)
{ }

FileDescriptor::FileDescriptor(PlatformHandleType aHandle)
: mHandle(INVALID_HANDLE), mHandleCreatedByOtherProcess(false),
  mHandleCreatedByOtherProcessWasUsed(false)
{
  DuplicateInCurrentProcess(aHandle);
}

void
FileDescriptor::DuplicateInCurrentProcess(PlatformHandleType aHandle)
{
  MOZ_ASSERT_IF(mHandleCreatedByOtherProcess,
                mHandleCreatedByOtherProcessWasUsed);

  if (IsValid(aHandle)) {
    PlatformHandleType newHandle;
#ifdef XP_WIN
    if (DuplicateHandle(GetCurrentProcess(), aHandle, GetCurrentProcess(),
                        &newHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
#else // XP_WIN
    if ((newHandle = dup(aHandle)) != INVALID_HANDLE) {
#endif
      mHandle = newHandle;
      return;
    }
    NS_WARNING("Failed to duplicate file handle for current process!");
  }

  mHandle = INVALID_HANDLE;
}

void
FileDescriptor::CloseCurrentProcessHandle()
{
  MOZ_ASSERT_IF(mHandleCreatedByOtherProcess,
                mHandleCreatedByOtherProcessWasUsed);

  // Don't actually close handles created by another process.
  if (mHandleCreatedByOtherProcess) {
    return;
  }

  if (IsValid()) {
#ifdef XP_WIN
    if (!CloseHandle(mHandle)) {
      NS_WARNING("Failed to close file handle for current process!");
    }
#else // XP_WIN
    HANDLE_EINTR(close(mHandle));
#endif
    mHandle = INVALID_HANDLE;
  }
}

FileDescriptor::PickleType
FileDescriptor::ShareTo(const FileDescriptor::IPDLPrivate&,
                        FileDescriptor::ProcessHandle aOtherProcess) const
{
  PlatformHandleType newHandle;
#ifdef XP_WIN
  if (IsValid()) {
    if (DuplicateHandle(GetCurrentProcess(), mHandle, aOtherProcess,
                        &newHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
      return newHandle;
    }
    NS_WARNING("Failed to duplicate file handle for other process!");
  }
  return INVALID_HANDLE;
#else // XP_WIN
  if (IsValid()) {
    newHandle = dup(mHandle);
    if (IsValid(newHandle)) {
      return base::FileDescriptor(newHandle, /* auto_close */ true);
    }
    NS_WARNING("Failed to duplicate file handle for other process!");
  }
  return base::FileDescriptor();
#endif

  MOZ_CRASH("Must not get here!");
  return PickleType();
}

// static
bool
FileDescriptor::IsValid(PlatformHandleType aHandle)
{
  return aHandle != INVALID_HANDLE;
}
