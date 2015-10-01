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

// Returns true if the shift key is currently pressed.
bool IsShiftPressed();

// Returns true if the ctrl key is currently pressed.
bool IsCtrlPressed();

// Returns true if the alt key is currently pressed.
bool IsAltPressed();

// Use the Win32 API FormatMessage() function to generate a string, using
// Windows's default Message Compiled resources; ignoring the inserts.
std::wstring FormatMessage(unsigned messageid);

// Uses the last Win32 error to generate a human readable message string.
std::wstring FormatLastWin32Error();

}  // namespace win_util

#endif  // BASE_WIN_UTIL_H__
