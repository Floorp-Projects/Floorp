// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win_util.h"

#include <map>
#include <sddl.h>

#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/tracked.h"

namespace win_util {

#define SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(struct_name, member) \
    offsetof(struct_name, member) + \
    (sizeof static_cast<struct_name*>(NULL)->member)
#define NONCLIENTMETRICS_SIZE_PRE_VISTA \
    SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(NONCLIENTMETRICS, lfMessageFont)

void GetNonClientMetrics(NONCLIENTMETRICS* metrics) {
  DCHECK(metrics);

  static const UINT SIZEOF_NONCLIENTMETRICS =
      (GetWinVersion() >= WINVERSION_VISTA) ?
      sizeof(NONCLIENTMETRICS) : NONCLIENTMETRICS_SIZE_PRE_VISTA;
  metrics->cbSize = SIZEOF_NONCLIENTMETRICS;
  const bool success = !!SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
                                              SIZEOF_NONCLIENTMETRICS, metrics,
                                              0);
  DCHECK(success);
}

WinVersion GetWinVersion() {
  static bool checked_version = false;
  static WinVersion win_version = WINVERSION_PRE_2000;
  if (!checked_version) {
    OSVERSIONINFOEX version_info;
    version_info.dwOSVersionInfoSize = sizeof version_info;
    GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version_info));
    if (version_info.dwMajorVersion == 5) {
      switch (version_info.dwMinorVersion) {
        case 0:
          win_version = WINVERSION_2000;
          break;
        case 1:
          win_version = WINVERSION_XP;
          break;
        case 2:
        default:
          win_version = WINVERSION_SERVER_2003;
          break;
      }
    } else if (version_info.dwMajorVersion == 6) {
      if (version_info.wProductType != VER_NT_WORKSTATION) {
        // 2008 is 6.0, and 2008 R2 is 6.1.
        win_version = WINVERSION_2008;
      } else {
        if (version_info.dwMinorVersion == 0) {
          win_version = WINVERSION_VISTA;
        } else {
          win_version = WINVERSION_WIN7;
        }
      }
    } else if (version_info.dwMajorVersion > 6) {
      win_version = WINVERSION_WIN7;
    }
    checked_version = true;
  }
  return win_version;
}

void GetServicePackLevel(int* major, int* minor) {
  DCHECK(major && minor);
  static bool checked_version = false;
  static int service_pack_major = -1;
  static int service_pack_minor = -1;
  if (!checked_version) {
    OSVERSIONINFOEX version_info = {0};
    version_info.dwOSVersionInfoSize = sizeof(version_info);
    GetVersionEx(reinterpret_cast<OSVERSIONINFOW*>(&version_info));
    service_pack_major = version_info.wServicePackMajor;
    service_pack_minor = version_info.wServicePackMinor;
    checked_version = true;
  }

  *major = service_pack_major;
  *minor = service_pack_minor;
}

bool AddAccessToKernelObject(HANDLE handle, WELL_KNOWN_SID_TYPE known_sid,
                             ACCESS_MASK access) {
  PSECURITY_DESCRIPTOR descriptor = NULL;
  PACL old_dacl = NULL;
  PACL new_dacl = NULL;

  if (ERROR_SUCCESS != GetSecurityInfo(handle, SE_KERNEL_OBJECT,
            DACL_SECURITY_INFORMATION, NULL, NULL, &old_dacl, NULL,
            &descriptor))
    return false;

  BYTE sid[SECURITY_MAX_SID_SIZE] = {0};
  DWORD size_sid = SECURITY_MAX_SID_SIZE;

  if (known_sid == WinSelfSid) {
    // We hijack WinSelfSid when we want to add the current user instead of
    // a known sid.
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token)) {
      LocalFree(descriptor);
      return false;
    }

    DWORD size = sizeof(TOKEN_USER) + size_sid;
    TOKEN_USER* token_user = reinterpret_cast<TOKEN_USER*>(new BYTE[size]);
    scoped_ptr<TOKEN_USER> token_user_ptr(token_user);
    BOOL ret = GetTokenInformation(token, TokenUser, token_user, size, &size);

    CloseHandle(token);

    if (!ret) {
      LocalFree(descriptor);
      return false;
    }
    memcpy(sid, token_user->User.Sid, size_sid);
  } else {
    if (!CreateWellKnownSid(known_sid , NULL, sid, &size_sid)) {
      LocalFree(descriptor);
      return false;
    }
  }

  EXPLICIT_ACCESS new_access = {0};
  new_access.grfAccessMode = GRANT_ACCESS;
  new_access.grfAccessPermissions = access;
  new_access.grfInheritance = NO_INHERITANCE;

  new_access.Trustee.pMultipleTrustee = NULL;
  new_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
  new_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  new_access.Trustee.ptstrName = reinterpret_cast<LPWSTR>(&sid);

  if (ERROR_SUCCESS != SetEntriesInAcl(1, &new_access, old_dacl, &new_dacl)) {
    LocalFree(descriptor);
    return false;
  }

  DWORD result = SetSecurityInfo(handle, SE_KERNEL_OBJECT,
                                 DACL_SECURITY_INFORMATION, NULL, NULL,
                                 new_dacl, NULL);

  LocalFree(new_dacl);
  LocalFree(descriptor);

  if (ERROR_SUCCESS != result)
    return false;

  return true;
}

bool GetUserSidString(std::wstring* user_sid) {
  // Get the current token.
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;
  ScopedHandle token_scoped(token);

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  scoped_ptr<TOKEN_USER> user(reinterpret_cast<TOKEN_USER*>(new BYTE[size]));

  if (!::GetTokenInformation(token, TokenUser, user.get(), size, &size))
    return false;

  if (!user->User.Sid)
    return false;

  // Convert the data to a string.
  wchar_t* sid_string;
  if (!::ConvertSidToStringSid(user->User.Sid, &sid_string))
    return false;

  *user_sid = sid_string;

  ::LocalFree(sid_string);

  return true;
}

bool IsShiftPressed() {
  return (::GetKeyState(VK_SHIFT) & 0x80) == 0x80;
}

bool IsCtrlPressed() {
  return (::GetKeyState(VK_CONTROL) & 0x80) == 0x80;
}

bool IsAltPressed() {
  return (::GetKeyState(VK_MENU) & 0x80) == 0x80;
}

std::wstring FormatMessage(unsigned messageid) {
  wchar_t* string_buffer = NULL;
  unsigned string_length = ::FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, NULL, messageid, 0,
      reinterpret_cast<wchar_t *>(&string_buffer), 0, NULL);

  std::wstring formatted_string;
  if (string_buffer) {
    formatted_string = string_buffer;
    LocalFree(reinterpret_cast<HLOCAL>(string_buffer));
  } else {
    // The formating failed. simply convert the message value into a string.
    SStringPrintf(&formatted_string, L"message number %d", messageid);
  }
  return formatted_string;
}

std::wstring FormatLastWin32Error() {
  return FormatMessage(GetLastError());
}

}  // namespace win_util

#ifdef _MSC_VER
//
// If the ASSERT below fails, please install Visual Studio 2005 Service Pack 1.
//
extern char VisualStudio2005ServicePack1Detection[10];
COMPILE_ASSERT(sizeof(&VisualStudio2005ServicePack1Detection) == sizeof(void*),
               VS2005SP1Detect);
//
// Chrome requires at least Service Pack 1 for Visual Studio 2005.
//
#endif  // _MSC_VER

#if 0
#error You must install the Windows 2008 or Vista Software Development Kit and \
set it as your default include path to build this library. You can grab it by \
searching for "download windows sdk 2008" in your favorite web search engine.  \
Also make sure you register the SDK with Visual Studio, by selecting \
"Integrate Windows SDK with Visual Studio 2005" from the Windows SDK \
menu (see Start - All Programs - Microsoft Windows SDK - \
Visual Studio Registration).
#endif
