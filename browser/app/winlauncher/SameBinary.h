/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SameBinary_h
#define mozilla_SameBinary_h

#include "mozilla/WinHeaderOnlyUtils.h"
#include "mozilla/NativeNt.h"
#include "nsWindowsHelpers.h"

namespace mozilla {

class ProcessImagePath final {
  PathType mType;
  LauncherVoidResult mLastError;

  // Using a larger buffer because an NT path may exceed MAX_PATH.
  WCHAR mPathBuffer[(MAX_PATH * 2) + 1];

 public:
  // Initialize with an NT path string of a given process handle
  explicit ProcessImagePath(const nsAutoHandle& aProcess)
      : mType(PathType::eNtPath), mLastError(Ok()) {
    DWORD len = mozilla::ArrayLength(mPathBuffer);
    if (!::QueryFullProcessImageNameW(aProcess.get(), PROCESS_NAME_NATIVE,
                                      mPathBuffer, &len)) {
      mLastError = LAUNCHER_ERROR_FROM_LAST();
      return;
    }
  }

  // Initizlize with a DOS path string of a given imagebase address
  explicit ProcessImagePath(HMODULE aImageBase)
      : mType(PathType::eDosPath), mLastError(Ok()) {
    DWORD len = ::GetModuleFileNameW(aImageBase, mPathBuffer,
                                     mozilla::ArrayLength(mPathBuffer));
    if (!len || len == mozilla::ArrayLength(mPathBuffer)) {
      mLastError = LAUNCHER_ERROR_FROM_LAST();
      return;
    }
  }

  bool IsError() const { return mLastError.isErr(); }

  const WindowsErrorType& GetError() const { return mLastError.inspectErr(); }

  FileUniqueId GetId() const { return FileUniqueId(mPathBuffer, mType); }

  bool CompareNtPaths(const ProcessImagePath& aOther) const {
    if (mLastError.isErr() || aOther.mLastError.isErr() ||
        mType != PathType::eNtPath || aOther.mType != PathType::eNtPath) {
      return false;
    }

    UNICODE_STRING path1, path2;
    ::RtlInitUnicodeString(&path1, mPathBuffer);
    ::RtlInitUnicodeString(&path2, aOther.mPathBuffer);
    return !!::RtlEqualUnicodeString(&path1, &path2, TRUE);
  }
};

enum class ImageFileCompareOption {
  Default,
  CompareNtPathsOnly,
};

static inline mozilla::LauncherResult<bool> IsSameBinaryAsParentProcess(
    ImageFileCompareOption aOption = ImageFileCompareOption::Default) {
  mozilla::LauncherResult<DWORD> parentPid = mozilla::nt::GetParentProcessId();
  if (parentPid.isErr()) {
    return parentPid.propagateErr();
  }

  nsAutoHandle parentProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                           FALSE, parentPid.unwrap()));
  if (!parentProcess.get()) {
    DWORD err = ::GetLastError();
    if (err == ERROR_INVALID_PARAMETER || err == ERROR_ACCESS_DENIED) {
      // In the ERROR_INVALID_PARAMETER case, the process identified by
      // parentPid has already exited. This is a common case when the parent
      // process is not Firefox, thus we should return false instead of erroring
      // out.
      // The ERROR_ACCESS_DENIED case can happen when the parent process is
      // something that we don't have permission to query. For example, we may
      // encounter this when Firefox is launched by the Windows Task Scheduler.
      return false;
    }

    return LAUNCHER_ERROR_FROM_WIN32(err);
  }

  ProcessImagePath parentExe(parentProcess);
  if (parentExe.IsError()) {
    return ::mozilla::Err(parentExe.GetError());
  }

  if (aOption == ImageFileCompareOption::Default) {
    bool skipFileIdComparison = false;

    FileUniqueId id1 = parentExe.GetId();
    if (id1.IsError()) {
      // We saw a number of Win7 users failed to call NtOpenFile with
      // STATUS_OBJECT_PATH_NOT_FOUND for an unknown reason.  In this
      // particular case, we fall back to the logic to compare NT path
      // strings instead of a file id which will not fail because we don't
      // need to open a file handle.
#if !defined(STATUS_OBJECT_PATH_NOT_FOUND)
      constexpr NTSTATUS STATUS_OBJECT_PATH_NOT_FOUND = 0xc000003a;
#endif
      const LauncherError& err = id1.GetError();
      if (err.mError !=
          WindowsError::FromNtStatus(STATUS_OBJECT_PATH_NOT_FOUND)) {
        return ::mozilla::Err(err);
      }

      skipFileIdComparison = true;
    }

    if (!skipFileIdComparison) {
      ProcessImagePath ourExe(nullptr);
      if (ourExe.IsError()) {
        return ::mozilla::Err(ourExe.GetError());
      }

      FileUniqueId id2 = ourExe.GetId();
      if (id2.IsError()) {
        return ::mozilla::Err(id2.GetError());
      }
      return id1 == id2;
    }
  }

  nsAutoHandle ourProcess(::GetCurrentProcess());
  ProcessImagePath ourExeNt(ourProcess);
  if (ourExeNt.IsError()) {
    return ::mozilla::Err(ourExeNt.GetError());
  }
  return parentExe.CompareNtPaths(ourExeNt);
}

}  // namespace mozilla

#endif  //  mozilla_SameBinary_h
