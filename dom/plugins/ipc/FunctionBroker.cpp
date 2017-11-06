/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FunctionBroker.h"
#include "FunctionBrokerParent.h"
#include "PluginQuirks.h"

#if defined(XP_WIN)
#include <commdlg.h>
#endif // defined(XP_WIN)

using namespace mozilla;
using namespace mozilla::ipc;
using namespace mozilla::plugins;

namespace mozilla {
namespace plugins {

template <int QuirkFlag>
static bool CheckQuirks(int aQuirks)
{
  return static_cast<bool>(aQuirks & QuirkFlag);
}

void FreeDestructor(void* aObj) { free(aObj); }

#if defined(XP_WIN)

/* GetKeyState */

typedef FunctionBroker<ID_GetKeyState,
                       decltype(GetKeyState)> GetKeyStateFB;

template<>
ShouldHookFunc* const
GetKeyStateFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_GETKEYSTATE>;

/* SetCursorPos */

typedef FunctionBroker<ID_SetCursorPos,
                       decltype(SetCursorPos)> SetCursorPosFB;

/* GetSaveFileNameW */

typedef FunctionBroker<ID_GetSaveFileNameW,
                             decltype(GetSaveFileNameW)> GetSaveFileNameWFB;

// Remember files granted access in the chrome process
static void GrantFileAccess(base::ProcessId aClientId, LPOPENFILENAME& aLpofn,
                            bool isSave)
{
#if defined(MOZ_SANDBOX)
  if (aLpofn->Flags & OFN_ALLOWMULTISELECT) {
    // We only support multiselect with the OFN_EXPLORER flag.
    // This guarantees that ofn.lpstrFile follows the pattern below.
    MOZ_ASSERT(aLpofn->Flags & OFN_EXPLORER);

    // lpstrFile is one of two things:
    // 1. A null terminated full path to a file, or
    // 2. A path to a folder, followed by a NULL, followed by a
    // list of file names, each NULL terminated, followed by an
    // additional NULL (so it is also double-NULL terminated).
    std::wstring path = std::wstring(aLpofn->lpstrFile);
    MOZ_ASSERT(aLpofn->nFileOffset > 0);
    // For condition #1, nFileOffset points to the file name in the path.
    // It will be preceeded by a non-NULL character from the path.
    if (aLpofn->lpstrFile[aLpofn->nFileOffset-1] != L'\0') {
      FunctionBrokerParent::GetSandboxPermissions()->GrantFileAccess(aClientId, path.c_str(), isSave);
    }
    else {
      // This is condition #2
      wchar_t* nextFile = aLpofn->lpstrFile + path.size() + 1;
      while (*nextFile != L'\0') {
        std::wstring nextFileStr(nextFile);
        std::wstring fullPath =
          path + std::wstring(L"\\") + nextFileStr;
        FunctionBrokerParent::GetSandboxPermissions()->GrantFileAccess(aClientId, fullPath.c_str(), isSave);
        nextFile += nextFileStr.size() + 1;
      }
    }
  }
  else {
    FunctionBrokerParent::GetSandboxPermissions()->GrantFileAccess(aClientId, aLpofn->lpstrFile, isSave);
  }
#else
  MOZ_ASSERT_UNREACHABLE("GetFileName IPC message is only available on "
                         "Windows builds with sandbox.");
#endif
}

template<> template<>
BOOL GetSaveFileNameWFB::RunFunction(GetSaveFileNameWFB::FunctionType* aOrigFunction,
                                    base::ProcessId aClientId,
                                    LPOPENFILENAMEW& aLpofn) const
{
  BOOL result = aOrigFunction(aLpofn);
  if (result) {
    // Record any file access permission that was just granted.
    GrantFileAccess(aClientId, aLpofn, true);
  }
  return result;
}

template<> template<>
struct GetSaveFileNameWFB::Response::Info::ShouldMarshal<0> { static const bool value = true; };

/* GetOpenFileNameW */

typedef FunctionBroker<ID_GetOpenFileNameW,
                       decltype(GetOpenFileNameW)> GetOpenFileNameWFB;

template<> template<>
BOOL GetOpenFileNameWFB::RunFunction(GetOpenFileNameWFB::FunctionType* aOrigFunction,
                                    base::ProcessId aClientId,
                                    LPOPENFILENAMEW& aLpofn) const
{
  BOOL result = aOrigFunction(aLpofn);
  if (result) {
    // Record any file access permission that was just granted.
    GrantFileAccess(aClientId, aLpofn, false);
  }
  return result;
}

template<> template<>
struct GetOpenFileNameWFB::Response::Info::ShouldMarshal<0> { static const bool value = true; };

#endif // defined(XP_WIN)

/*****************************************************************************/

#define FUN_HOOK(x) static_cast<FunctionHook*>(x)

void
AddBrokeredFunctionHooks(FunctionHookArray& aHooks)
{
  // We transfer ownership of the FunctionHook objects to the array.
#if defined(XP_WIN)
  aHooks[ID_GetKeyState] =
    FUN_HOOK(new GetKeyStateFB("user32.dll", "GetKeyState", &GetKeyState));
  aHooks[ID_SetCursorPos] =
    FUN_HOOK(new SetCursorPosFB("user32.dll", "SetCursorPos", &SetCursorPos));
  aHooks[ID_GetSaveFileNameW] =
    FUN_HOOK(new GetSaveFileNameWFB("comdlg32.dll", "GetSaveFileNameW",
                                    &GetSaveFileNameW));
  aHooks[ID_GetOpenFileNameW] =
    FUN_HOOK(new GetOpenFileNameWFB("comdlg32.dll", "GetOpenFileNameW",
                                    &GetOpenFileNameW));
#endif // defined(XP_WIN)
}

#undef FUN_HOOK

} // namespace plugins
} // namespace mozilla
