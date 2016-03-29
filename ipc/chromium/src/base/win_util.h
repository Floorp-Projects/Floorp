// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_UTIL_H__
#define BASE_WIN_UTIL_H__

#include <windows.h>
#include <aclapi.h>

#include <string>

#include "base/tracked.h"

namespace win_util {

// Uses the last Win32 error to generate a human readable message string.
std::wstring FormatLastWin32Error();

}  // namespace win_util

#endif  // BASE_WIN_UTIL_H__
