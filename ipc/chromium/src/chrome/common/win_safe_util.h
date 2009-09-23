// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WIN_SAFE_UTIL_H__
#define CHROME_COMMON_WIN_SAFE_UTIL_H__

#include <string>
#include <windows.h>

class FilePath;

namespace win_util {

// Open or run a downloaded file via the Windows shell, possibly showing first
// a consent dialog if the the file is deemed dangerous. This function is an
// enhancement over the OpenItemViaShell() function of win_util.h.
//
// The user consent dialog will be shown or not according to the windows
// execution policy defined in the registry which can be overridden per user.
// The mechanics of the policy are explained in the Microsoft Knowledge base
// number 883260: http://support.microsoft.com/kb/883260
//
// The 'hwnd' is the handle to the parent window. In case a dialog is displayed
// the parent window will be disabled since the dialog is meant to be modal.
// The 'window_title' is the text displayed on the title bar of the dialog. If
// you pass an empty string the dialog will have a generic 'windows security'
// name on the title bar.
//
// You must provide a valid 'full_path' to the file to be opened and a well
// formed url in 'source_url'. The url should identify the source of the file
// but does not have to be network-reachable. If the url is malformed a
// dialog will be shown telling the user that the file will be blocked.
//
// In the event that there is no default application registered for the file
// specified by 'full_path' it ask the user, via the Windows "Open With"
// dialog, for an application to use if 'ask_for_app' is true.
// Returns 'true' on successful open, 'false' otherwise.
bool SaferOpenItemViaShell(HWND hwnd, const std::wstring& window_title,
                           const FilePath& full_path,
                           const std::wstring& source_url, bool ask_for_app);

// Sets the Zone Identifier on the file to "Internet" (3). Returns true if the
// function succeeds, false otherwise. A failure is expected on system where
// the Zone Identifier is not supported, like a machine with a FAT32 filesystem.
// It should not be considered fatal.
bool SetInternetZoneIdentifier(const FilePath& full_path);

}  // namespace win_util

#endif // CHROME_COMMON_WIN_SAFE_UTIL_H__
