/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LaunchError.h"

#if defined(XP_WIN)
#  include "mozilla/WinHeaderOnlyUtils.h"
#endif

namespace mozilla::ipc {

LaunchError::LaunchError(const char* aFunction, OsError aError)
    : mFunction(aFunction), mError(aError) {}

#if defined(XP_WIN)
LaunchError::LaunchError(const char* aFunction, PRErrorCode aError)
    : mFunction(aFunction), mError((int)aError) {}

LaunchError::LaunchError(const char* aFunction, DWORD aError)
    : mFunction(aFunction),
      mError(WindowsError::FromWin32Error(aError).AsHResult()) {}
#endif  // defined(XP_WIN)

const char* LaunchError::FunctionName() const { return mFunction; }

OsError LaunchError::ErrorCode() const { return mError; }

}  // namespace mozilla::ipc
