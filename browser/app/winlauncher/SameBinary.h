/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SameBinary_h
#define mozilla_SameBinary_h

#include "mozilla/LauncherResult.h"
#include "mozilla/NativeNt.h"
#include "nsWindowsHelpers.h"

namespace mozilla {

static inline mozilla::LauncherResult<bool> IsSameBinaryAsParentProcess() {
  mozilla::LauncherResult<DWORD> parentPid = mozilla::nt::GetParentProcessId();
  if (parentPid.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(parentPid);
  }

  nsAutoHandle parentProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                           FALSE, parentPid.unwrap()));
  if (!parentProcess.get()) {
    DWORD err = ::GetLastError();
    if (err == ERROR_INVALID_PARAMETER) {
      // The process identified by parentPid has already exited. This is a
      // common case when the parent process is not Firefox, thus we should
      // return false instead of erroring out.
      return false;
    }

    return LAUNCHER_ERROR_FROM_WIN32(err);
  }

  // Using a larger buffer for this call because we're asking for the native NT
  // path, which may exceed MAX_PATH.
  WCHAR parentExe[(MAX_PATH * 2) + 1] = {};
  DWORD parentExeLen = mozilla::ArrayLength(parentExe);
  if (!::QueryFullProcessImageNameW(parentProcess.get(), PROCESS_NAME_NATIVE,
                                    parentExe, &parentExeLen)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  WCHAR ourExe[MAX_PATH + 1] = {};
  DWORD ourExeOk =
      ::GetModuleFileNameW(nullptr, ourExe, mozilla::ArrayLength(ourExe));
  if (!ourExeOk || ourExeOk == mozilla::ArrayLength(ourExe)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  mozilla::WindowsErrorResult<bool> isSame =
      mozilla::DoPathsPointToIdenticalFile(parentExe, ourExe,
                                           mozilla::PathType::eNtPath);
  if (isSame.isErr()) {
    return LAUNCHER_ERROR_FROM_MOZ_WINDOWS_ERROR(isSame.unwrapErr());
  }

  return isSame.unwrap();
}

}  // namespace mozilla

#endif  //  mozilla_SameBinary_h
