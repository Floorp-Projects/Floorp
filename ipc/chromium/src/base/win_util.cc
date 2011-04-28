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

bool GetLogonSessionOnlyDACL(SECURITY_DESCRIPTOR** security_descriptor) {
  // Get the current token.
  HANDLE token = NULL;
  if (!OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;
  ScopedHandle token_scoped(token);

  // Get the size of the TokenGroups structure.
  DWORD size = 0;
  BOOL result = GetTokenInformation(token, TokenGroups, NULL,  0, &size);
  if (result != FALSE && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    return false;

  // Get the data.
  scoped_ptr<TOKEN_GROUPS> token_groups;
  token_groups.reset(reinterpret_cast<TOKEN_GROUPS*>(new char[size]));

  if (!GetTokenInformation(token, TokenGroups, token_groups.get(), size, &size))
    return false;

  // Look for the logon sid.
  SID* logon_sid = NULL;
  for (unsigned int i = 0; i < token_groups->GroupCount ; ++i) {
    if ((token_groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) != 0) {
        logon_sid = static_cast<SID*>(token_groups->Groups[i].Sid);
        break;
    }
  }

  if (!logon_sid)
    return false;

  // Convert the data to a string.
  wchar_t* sid_string;
  if (!ConvertSidToStringSid(logon_sid, &sid_string))
    return false;

  static const wchar_t dacl_format[] = L"D:(A;OICI;GA;;;%ls)";
  wchar_t dacl[SECURITY_MAX_SID_SIZE + arraysize(dacl_format) + 1] = {0};
  wsprintf(dacl, dacl_format, sid_string);

  LocalFree(sid_string);

  // Convert the string to a security descriptor
  if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
      dacl,
      SDDL_REVISION_1,
      reinterpret_cast<PSECURITY_DESCRIPTOR*>(security_descriptor),
      NULL)) {
    return false;
  }

  return true;
}

#pragma warning(push)
#pragma warning(disable:4312 4244)
WNDPROC SetWindowProc(HWND hwnd, WNDPROC proc) {
  // The reason we don't return the SetwindowLongPtr() value is that it returns
  // the orignal window procedure and not the current one. I don't know if it is
  // a bug or an intended feature.
  WNDPROC oldwindow_proc =
      reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
  SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(proc));
  return oldwindow_proc;
}

void* SetWindowUserData(HWND hwnd, void* user_data) {
  return
      reinterpret_cast<void*>(SetWindowLongPtr(hwnd, GWLP_USERDATA,
          reinterpret_cast<LONG_PTR>(user_data)));
}

void* GetWindowUserData(HWND hwnd) {
  return reinterpret_cast<void*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

// Maps to the WNDPROC for a window that was active before the subclass was
// installed.
static const wchar_t* const kHandlerKey = L"__ORIGINAL_MESSAGE_HANDLER__";

bool IsSubclassed(HWND window, WNDPROC subclass_proc) {
  WNDPROC original_handler =
      reinterpret_cast<WNDPROC>(GetWindowLongPtr(window, GWLP_WNDPROC));
  return original_handler == subclass_proc;
}

bool Subclass(HWND window, WNDPROC subclass_proc) {
  WNDPROC original_handler =
      reinterpret_cast<WNDPROC>(GetWindowLongPtr(window, GWLP_WNDPROC));
  if (original_handler != subclass_proc) {
    win_util::SetWindowProc(window, subclass_proc);
    SetProp(window, kHandlerKey, (void*)original_handler);
    return true;
  }
  return false;
}

bool Unsubclass(HWND window, WNDPROC subclass_proc) {
  WNDPROC current_handler =
      reinterpret_cast<WNDPROC>(GetWindowLongPtr(window, GWLP_WNDPROC));
  if (current_handler == subclass_proc) {
    HANDLE original_handler = GetProp(window, kHandlerKey);
    if (original_handler) {
      RemoveProp(window, kHandlerKey);
      win_util::SetWindowProc(window,
                              reinterpret_cast<WNDPROC>(original_handler));
      return true;
    }
  }
  return false;
}

WNDPROC GetSuperclassWNDPROC(HWND window) {
  return reinterpret_cast<WNDPROC>(GetProp(window, kHandlerKey));
}

#pragma warning(pop)

bool IsShiftPressed() {
  return (::GetKeyState(VK_SHIFT) & 0x80) == 0x80;
}

bool IsCtrlPressed() {
  return (::GetKeyState(VK_CONTROL) & 0x80) == 0x80;
}

bool IsAltPressed() {
  return (::GetKeyState(VK_MENU) & 0x80) == 0x80;
}

std::wstring GetClassName(HWND window) {
  // GetClassNameW will return a truncated result (properly null terminated) if
  // the given buffer is not large enough.  So, it is not possible to determine
  // that we got the entire class name if the result is exactly equal to the
  // size of the buffer minus one.
  DWORD buffer_size = MAX_PATH;
  while (true) {
    std::wstring output;
    DWORD size_ret =
        GetClassNameW(window, WriteInto(&output, buffer_size), buffer_size);
    if (size_ret == 0)
      break;
    if (size_ret < (buffer_size - 1)) {
      output.resize(size_ret);
      return output;
    }
    buffer_size *= 2;
  }
  return std::wstring();  // error
}

bool UserAccountControlIsEnabled() {
  RegKey key(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System");
  DWORD uac_enabled;
  if (!key.ReadValueDW(L"EnableLUA", &uac_enabled))
    return true;
  // Users can set the EnableLUA value to something arbitrary, like 2, which
  // Vista will treat as UAC enabled, so we make sure it is not set to 0.
  return (uac_enabled != 0);
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

typedef std::map<HWND, tracked_objects::Location> HWNDInfoMap;
struct HWNDBirthMapTrait : public DefaultSingletonTraits<HWNDInfoMap> {
};
struct HWNDDeathMapTrait : public DefaultSingletonTraits<HWNDInfoMap> {
};

void NotifyHWNDCreation(const tracked_objects::Location& from_here, HWND hwnd) {
  HWNDInfoMap* birth_map = Singleton<HWNDInfoMap, HWNDBirthMapTrait>::get();
  HWNDInfoMap::iterator birth_iter = birth_map->find(hwnd);
  if (birth_iter != birth_map->end()) {
    birth_map->erase(birth_iter);

    // We have already seen this HWND, was it destroyed?
    HWNDInfoMap* death_map = Singleton<HWNDInfoMap, HWNDDeathMapTrait>::get();
    HWNDInfoMap::iterator death_iter = death_map->find(hwnd);
    if (death_iter == death_map->end()) {
      // We did not get a destruction notification.  The code is probably not
      // calling NotifyHWNDDestruction for that HWND.
      NOTREACHED() << "Creation of HWND reported for already tracked HWND. The "
                      "HWND destruction is probably not tracked properly. "
                      "Fix it!";
    } else {
      death_map->erase(death_iter);
    }
  }
  birth_map->insert(std::pair<HWND, tracked_objects::Location>(hwnd,
                                                              from_here));
}

void NotifyHWNDDestruction(const tracked_objects::Location& from_here,
                           HWND hwnd) {
  HWNDInfoMap* death_map = Singleton<HWNDInfoMap, HWNDDeathMapTrait>::get();
  HWNDInfoMap::iterator death_iter = death_map->find(hwnd);

  HWNDInfoMap* birth_map = Singleton<HWNDInfoMap, HWNDBirthMapTrait>::get();
  HWNDInfoMap::iterator birth_iter = birth_map->find(hwnd);

  if (death_iter != death_map->end()) {
    std::string allocation, first_delete, second_delete;
    if (birth_iter != birth_map->end())
      birth_iter->second.Write(true, true, &allocation);
    death_iter->second.Write(true, true, &first_delete);
    from_here.Write(true, true, &second_delete);
    LOG(FATAL) << "Double delete of an HWND. Please file a bug with info on "
        "how you got that assertion and the following information:\n"
        "Double delete of HWND 0x" << hwnd << "\n" <<
        "Allocated at " << allocation << "\n" <<
        "Deleted first at " << first_delete << "\n" <<
        "Deleted again at " << second_delete;
    death_map->erase(death_iter);
  }

  if (birth_iter == birth_map->end()) {
    NOTREACHED() << "Destruction of HWND reported for unknown HWND. The HWND "
                    "construction is probably not tracked properly. Fix it!";
  }
  death_map->insert(std::pair<HWND, tracked_objects::Location>(hwnd,
                                                               from_here));
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
