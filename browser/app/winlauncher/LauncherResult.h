/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LauncherResult_h
#define mozilla_LauncherResult_h

#include "mozilla/Result.h"
#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla {

struct LauncherError {
  LauncherError(const char* aFile, int aLine, WindowsError aWin32Error)
      : mFile(aFile), mLine(aLine), mError(aWin32Error) {}

  const char* mFile;
  int mLine;
  WindowsError mError;
};

template <typename T>
using LauncherResult = Result<T, LauncherError>;

using LauncherVoidResult = Result<Ok, LauncherError>;

}  // namespace mozilla

#define LAUNCHER_ERROR_GENERIC()           \
  ::mozilla::Err(::mozilla::LauncherError( \
      __FILE__, __LINE__, ::mozilla::WindowsError::CreateGeneric()))

#define LAUNCHER_ERROR_FROM_WIN32(err)     \
  ::mozilla::Err(::mozilla::LauncherError( \
      __FILE__, __LINE__, ::mozilla::WindowsError::FromWin32Error(err)))

#define LAUNCHER_ERROR_FROM_LAST()         \
  ::mozilla::Err(::mozilla::LauncherError( \
      __FILE__, __LINE__,                  \
      ::mozilla::WindowsError::FromWin32Error(::GetLastError())))

#define LAUNCHER_ERROR_FROM_NTSTATUS(ntstatus) \
  ::mozilla::Err(::mozilla::LauncherError(     \
      __FILE__, __LINE__, ::mozilla::WindowsError::FromNtStatus(ntstatus)))

#define LAUNCHER_ERROR_FROM_HRESULT(hresult) \
  ::mozilla::Err(::mozilla::LauncherError(   \
      __FILE__, __LINE__, ::mozilla::WindowsError::FromHResult(hresult)))

// This macro enables moving of a mozilla::LauncherError from a
// mozilla::LauncherResult<Foo> into a mozilla::LauncherResult<Bar>
#define LAUNCHER_ERROR_FROM_RESULT(result) ::mozilla::Err(result.unwrapErr())

// This macro wraps the supplied WindowsError with a LauncherError
#define LAUNCHER_ERROR_FROM_MOZ_WINDOWS_ERROR(err) \
  ::mozilla::Err(::mozilla::LauncherError(__FILE__, __LINE__, err))

#endif  // mozilla_LauncherResult_h
