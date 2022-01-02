/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DllBlocklistInit_h
#define mozilla_DllBlocklistInit_h

#include <windows.h>

#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla {

LauncherVoidResultWithLineInfo InitializeDllBlocklistOOP(
    const wchar_t* aFullImagePath, HANDLE aChildProcess,
    const IMAGE_THUNK_DATA* aCachedNtdllThunk);

LauncherVoidResultWithLineInfo InitializeDllBlocklistOOPFromLauncher(
    const wchar_t* aFullImagePath, HANDLE aChildProcess);

}  // namespace mozilla

#endif  // mozilla_DllBlocklistInit_h
