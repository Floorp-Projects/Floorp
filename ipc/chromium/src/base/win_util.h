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

// NOTE: Keep these in order so callers can do things like
// "if (GetWinVersion() > WINVERSION_2000) ...".  It's OK to change the values,
// though.
enum WinVersion {
  WINVERSION_PRE_2000 = 0,  // Not supported
  WINVERSION_2000 = 1,
  WINVERSION_XP = 2,
  WINVERSION_SERVER_2003 = 3,
  WINVERSION_VISTA = 4,
  WINVERSION_2008 = 5,
  WINVERSION_WIN7 = 6
};

void GetNonClientMetrics(NONCLIENTMETRICS* metrics);

// Returns the running version of Windows.
WinVersion GetWinVersion();

// Returns the major and minor version of the service pack installed.
void GetServicePackLevel(int* major, int* minor);

// Adds an ACE in the DACL of the object referenced by handle. The ACE is
// granting |access| to the user |known_sid|.
// If |known_sid| is WinSelfSid, the sid of the current user will be added to
// the DACL.
bool AddAccessToKernelObject(HANDLE handle, WELL_KNOWN_SID_TYPE known_sid,
                             ACCESS_MASK access);

// Returns the string representing the current user sid.
bool GetUserSidString(std::wstring* user_sid);

// Creates a security descriptor with a DACL that has one ace giving full
// access to the current logon session.
// The security descriptor returned must be freed using LocalFree.
// The function returns true if it succeeds, false otherwise.
bool GetLogonSessionOnlyDACL(SECURITY_DESCRIPTOR** security_descriptor);

// Useful for subclassing a HWND.  Returns the previous window procedure.
WNDPROC SetWindowProc(HWND hwnd, WNDPROC wndproc);

// Returns true if the existing window procedure is the same as |subclass_proc|.
bool IsSubclassed(HWND window, WNDPROC subclass_proc);

// Subclasses a window, replacing its existing window procedure with the
// specified one. Returns true if the current window procedure was replaced,
// false if the window has already been subclassed with the specified
// subclass procedure.
bool Subclass(HWND window, WNDPROC subclass_proc);

// Unsubclasses a window subclassed using Subclass. Returns true if
// the window was subclassed with the specified |subclass_proc| and the window
// was successfully unsubclassed, false if the window's window procedure is not
// |subclass_proc|.
bool Unsubclass(HWND window, WNDPROC subclass_proc);

// Retrieves the original WNDPROC of a window subclassed using
// SubclassWindow.
WNDPROC GetSuperclassWNDPROC(HWND window);

// Pointer-friendly wrappers around Get/SetWindowLong(..., GWLP_USERDATA, ...)
// Returns the previously set value.
void* SetWindowUserData(HWND hwnd, void* user_data);
void* GetWindowUserData(HWND hwnd);

// Returns true if the shift key is currently pressed.
bool IsShiftPressed();

// Returns true if the ctrl key is currently pressed.
bool IsCtrlPressed();

// Returns true if the alt key is currently pressed.
bool IsAltPressed();

// A version of the GetClassNameW API that returns the class name in an
// std::wstring.  An empty result indicates a failure to get the class name.
std::wstring GetClassName(HWND window);

// Returns false if user account control (UAC) has been disabled with the
// EnableLUA registry flag. Returns true if user account control is enabled.
// NOTE: The EnableLUA registry flag, which is ignored on Windows XP
// machines, might still exist and be set to 0 (UAC disabled), in which case
// this function will return false. You should therefore check this flag only
// if the OS is Vista.
bool UserAccountControlIsEnabled();

// Use the Win32 API FormatMessage() function to generate a string, using
// Windows's default Message Compiled resources; ignoring the inserts.
std::wstring FormatMessage(unsigned messageid);

// Uses the last Win32 error to generate a human readable message string.
std::wstring FormatLastWin32Error();

// These 2 methods are used to track HWND creation/destruction to investigate
// a mysterious crasher http://crbugs.com/4714 (crasher in on NCDestroy) that
// might be caused by a multiple delete of an HWND.
void NotifyHWNDCreation(const tracked_objects::Location& from_here, HWND hwnd);
void NotifyHWNDDestruction(const tracked_objects::Location& from_here,
                           HWND hwnd);

#define TRACK_HWND_CREATION(hwnd) win_util::NotifyHWNDCreation(FROM_HERE, hwnd)
#define TRACK_HWND_DESTRUCTION(hwnd) \
    win_util::NotifyHWNDDestruction(FROM_HERE, hwnd)

}  // namespace win_util

#endif  // BASE_WIN_UTIL_H__
