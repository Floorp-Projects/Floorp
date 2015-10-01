// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win_util.h"

#include <map>
#include <sddl.h>

#include "base/logging.h"
#include "base/registry.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/tracked.h"

namespace win_util {

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
