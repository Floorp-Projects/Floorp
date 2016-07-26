/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileDescriptor.h"

#include "mozilla/Assertions.h"
#include "mozilla/Move.h"
#include "nsDebug.h"

#ifdef XP_WIN

#include <windows.h>
#include "ProtocolUtils.h"
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
  : mHandle(INVALID_HANDLE)
{
}

FileDescriptor::FileDescriptor(const FileDescriptor& aOther)
  : mHandle(INVALID_HANDLE)
{
  Assign(aOther);
}

FileDescriptor::FileDescriptor(FileDescriptor&& aOther)
  : mHandle(INVALID_HANDLE)
{
  *this = mozilla::Move(aOther);
}

FileDescriptor::FileDescriptor(PlatformHandleType aHandle)
  : mHandle(INVALID_HANDLE)
{
  mHandle = Clone(aHandle);
}

FileDescriptor::FileDescriptor(const IPDLPrivate&, const PickleType& aPickle)
  : mHandle(INVALID_HANDLE)
{
#ifdef XP_WIN
  mHandle = aPickle;
#else
  mHandle = aPickle.fd;
#endif
}

FileDescriptor::~FileDescriptor()
{
  Close();
}

FileDescriptor&
FileDescriptor::operator=(const FileDescriptor& aOther)
{
  if (this != &aOther) {
    Assign(aOther);
  }
  return *this;
}

FileDescriptor&
FileDescriptor::operator=(FileDescriptor&& aOther)
{
  if (this != &aOther) {
    Close();
    mHandle = aOther.mHandle;
    aOther.mHandle = INVALID_HANDLE;
  }
  return *this;
}

bool
FileDescriptor::IsValid() const
{
  return IsValid(mHandle);
}

void
FileDescriptor::Assign(const FileDescriptor& aOther)
{
  Close();
  mHandle = Clone(aOther.mHandle);
}

void
FileDescriptor::Close()
{
  Close(mHandle);
  mHandle = INVALID_HANDLE;
}

FileDescriptor::PickleType
FileDescriptor::ShareTo(const FileDescriptor::IPDLPrivate&,
                        FileDescriptor::ProcessId aTargetPid) const
{
  PlatformHandleType newHandle;
#ifdef XP_WIN
  if (IsValid()) {
    if (mozilla::ipc::DuplicateHandle(mHandle, aTargetPid, &newHandle, 0,
                                      DUPLICATE_SAME_ACCESS)) {
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
}

FileDescriptor::UniquePlatformHandle
FileDescriptor::ClonePlatformHandle() const
{
  return UniquePlatformHandle(Clone(mHandle));
}

bool
FileDescriptor::operator==(const FileDescriptor& aOther) const
{
  return mHandle == aOther.mHandle;
}

// static
bool
FileDescriptor::IsValid(PlatformHandleType aHandle)
{
  return aHandle != INVALID_HANDLE;
}

// static
FileDescriptor::PlatformHandleType
FileDescriptor::Clone(PlatformHandleType aHandle)
{
  if (!IsValid(aHandle)) {
    return INVALID_HANDLE;
  }
  FileDescriptor::PlatformHandleType newHandle;
#ifdef XP_WIN
  if (::DuplicateHandle(GetCurrentProcess(), aHandle, GetCurrentProcess(),
                        &newHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
#else // XP_WIN
  if ((newHandle = dup(aHandle)) != INVALID_HANDLE) {
#endif
        return newHandle;
  }
  NS_WARNING("Failed to duplicate file handle for current process!");
  return INVALID_HANDLE;
}

// static
void
FileDescriptor::Close(PlatformHandleType aHandle)
{
  if (IsValid(aHandle)) {
#ifdef XP_WIN
    if (!CloseHandle(aHandle)) {
      NS_WARNING("Failed to close file handle for current process!");
    }
#else // XP_WIN
    HANDLE_EINTR(close(aHandle));
#endif
  }
}

FileDescriptor::PlatformHandleHelper::PlatformHandleHelper(FileDescriptor::PlatformHandleType aHandle)
  :mHandle(aHandle)
{
}

FileDescriptor::PlatformHandleHelper::PlatformHandleHelper(std::nullptr_t)
  :mHandle(INVALID_HANDLE)
{
}

bool
FileDescriptor::PlatformHandleHelper::operator!=(std::nullptr_t) const
{
  return mHandle != INVALID_HANDLE;
}

FileDescriptor::PlatformHandleHelper::operator FileDescriptor::PlatformHandleType () const
{
  return mHandle;
}

#ifdef XP_WIN
FileDescriptor::PlatformHandleHelper::operator std::intptr_t () const
{
  return reinterpret_cast<std::intptr_t>(mHandle);
}
#endif

void
FileDescriptor::PlatformHandleDeleter::operator()(FileDescriptor::PlatformHandleHelper aHelper)
{
  FileDescriptor::Close(aHelper);
}
