/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FunctionHook.h"
#include "FunctionBroker.h"
#include "nsClassHashtable.h"
#include "mozilla/ClearOnShutdown.h"

#if defined(XP_WIN)
#include <shlobj.h>
#endif

namespace mozilla {
namespace plugins {

StaticAutoPtr<FunctionHookArray> FunctionHook::sFunctionHooks;

bool AlwaysHook(int) { return true; }

FunctionHookArray*
FunctionHook::GetHooks()
{
  if (sFunctionHooks) {
    return sFunctionHooks;
  }

  // sFunctionHooks is the StaticAutoPtr to the singleton array of FunctionHook
  // objects.  We free it by clearing the StaticAutoPtr on shutdown.
  sFunctionHooks = new FunctionHookArray();
  ClearOnShutdown(&sFunctionHooks);
  sFunctionHooks->SetLength(ID_FunctionHookCount);

  AddFunctionHooks(*sFunctionHooks);
  AddBrokeredFunctionHooks(*sFunctionHooks);
  return sFunctionHooks;
}

void
FunctionHook::HookFunctions(int aQuirks)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Plugin);
  FunctionHookArray* hooks = FunctionHook::GetHooks();
  MOZ_ASSERT(hooks);
  for(size_t i=0; i < hooks->Length(); ++i) {
    FunctionHook* mhb = hooks->ElementAt(i);
    // Check that the FunctionHook array is in the same order as the
    // FunctionHookId enum.
    MOZ_ASSERT((size_t)mhb->FunctionId() == i);
    mhb->Register(aQuirks);
  }
}

#if defined(XP_WIN)

// This cache is created when a DLL is registered with a FunctionHook.
// It is cleared on a call to ClearDllInterceptorCache().  It
// must be freed before exit to avoid leaks.
typedef nsClassHashtable<nsCStringHashKey, WindowsDllInterceptor> DllInterceptors;
DllInterceptors* sDllInterceptorCache = nullptr;

WindowsDllInterceptor*
FunctionHook::GetDllInterceptorFor(const char* aModuleName)
{
  if (!sDllInterceptorCache) {
    sDllInterceptorCache = new DllInterceptors();
  }

  WindowsDllInterceptor* ret =
    sDllInterceptorCache->LookupOrAdd(nsCString(aModuleName), aModuleName);
  MOZ_ASSERT(ret);
  return ret;
}

void
FunctionHook::ClearDllInterceptorCache()
{
  delete sDllInterceptorCache;
  sDllInterceptorCache = nullptr;
}

// Hooking CreateFileW for protected-mode magic
static WindowsDllInterceptor sKernel32Intercept;
typedef HANDLE (WINAPI *CreateFileWPtr)(LPCWSTR aFname, DWORD aAccess,
                                        DWORD aShare,
                                        LPSECURITY_ATTRIBUTES aSecurity,
                                        DWORD aCreation, DWORD aFlags,
                                        HANDLE aFTemplate);
static CreateFileWPtr sCreateFileWStub = nullptr;
typedef HANDLE (WINAPI *CreateFileAPtr)(LPCSTR aFname, DWORD aAccess,
                                        DWORD aShare,
                                        LPSECURITY_ATTRIBUTES aSecurity,
                                        DWORD aCreation, DWORD aFlags,
                                        HANDLE aFTemplate);
static CreateFileAPtr sCreateFileAStub = nullptr;

// Windows 8 RTM (kernelbase's version is 6.2.9200.16384) doesn't call
// CreateFileW from CreateFileA.
// So we hook CreateFileA too to use CreateFileW hook.
static HANDLE WINAPI
CreateFileAHookFn(LPCSTR aFname, DWORD aAccess, DWORD aShare,
                  LPSECURITY_ATTRIBUTES aSecurity, DWORD aCreation, DWORD aFlags,
                  HANDLE aFTemplate)
{
  while (true) { // goto out
    // Our hook is for mms.cfg into \Windows\System32\Macromed\Flash
    // We don't require supporting too long path.
    WCHAR unicodeName[MAX_PATH];
    size_t len = strlen(aFname);

    if (len >= MAX_PATH) {
      break;
    }

    // We call to CreateFileW for workaround of Windows 8 RTM
    int newLen = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, aFname,
                                     len, unicodeName, MAX_PATH);
    if (newLen == 0 || newLen >= MAX_PATH) {
      break;
    }
    unicodeName[newLen] = '\0';

    return CreateFileW(unicodeName, aAccess, aShare, aSecurity, aCreation, aFlags, aFTemplate);
  }

  return sCreateFileAStub(aFname, aAccess, aShare, aSecurity, aCreation, aFlags,
                          aFTemplate);
}

static bool
GetLocalLowTempPath(size_t aLen, LPWSTR aPath)
{
  NS_NAMED_LITERAL_STRING(tempname, "\\Temp");
  LPWSTR path;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0,
                                     nullptr, &path))) {
    if (wcslen(path) + tempname.Length() < aLen) {
        wcscpy(aPath, path);
        wcscat(aPath, tempname.get());
        CoTaskMemFree(path);
        return true;
    }
    CoTaskMemFree(path);
  }

  // XP doesn't support SHGetKnownFolderPath and LocalLow
  if (!GetTempPathW(aLen, aPath)) {
    return false;
  }
  return true;
}

HANDLE WINAPI
CreateFileWHookFn(LPCWSTR aFname, DWORD aAccess, DWORD aShare,
                  LPSECURITY_ATTRIBUTES aSecurity, DWORD aCreation, DWORD aFlags,
                  HANDLE aFTemplate)
{
  static const WCHAR kConfigFile[] = L"mms.cfg";
  static const size_t kConfigLength = ArrayLength(kConfigFile) - 1;

  while (true) { // goto out, in sheep's clothing
    size_t len = wcslen(aFname);
    if (len < kConfigLength) {
      break;
    }
    if (wcscmp(aFname + len - kConfigLength, kConfigFile) != 0) {
      break;
    }

    // This is the config file we want to rewrite
    WCHAR tempPath[MAX_PATH+1];
    if (GetLocalLowTempPath(MAX_PATH, tempPath) == 0) {
      break;
    }
    WCHAR tempFile[MAX_PATH+1];
    if (GetTempFileNameW(tempPath, L"fx", 0, tempFile) == 0) {
      break;
    }
    HANDLE replacement =
      sCreateFileWStub(tempFile, GENERIC_READ | GENERIC_WRITE, aShare,
                       aSecurity, TRUNCATE_EXISTING,
                       FILE_ATTRIBUTE_TEMPORARY |
                         FILE_FLAG_DELETE_ON_CLOSE,
                       NULL);
    if (replacement == INVALID_HANDLE_VALUE) {
      break;
    }

    HANDLE original = sCreateFileWStub(aFname, aAccess, aShare, aSecurity,
                                       aCreation, aFlags, aFTemplate);
    if (original != INVALID_HANDLE_VALUE) {
      // copy original to replacement
      static const size_t kBufferSize = 1024;
      char buffer[kBufferSize];
      DWORD bytes;
      while (ReadFile(original, buffer, kBufferSize, &bytes, NULL)) {
        if (bytes == 0) {
          break;
        }
        DWORD wbytes;
        WriteFile(replacement, buffer, bytes, &wbytes, NULL);
        if (bytes < kBufferSize) {
          break;
        }
      }
      CloseHandle(original);
    }
    static const char kSettingString[] = "\nProtectedMode=0\n";
    DWORD wbytes;
    WriteFile(replacement, static_cast<const void*>(kSettingString),
              sizeof(kSettingString) - 1, &wbytes, NULL);
    SetFilePointer(replacement, 0, NULL, FILE_BEGIN);
    return replacement;
  }
  return sCreateFileWStub(aFname, aAccess, aShare, aSecurity, aCreation, aFlags,
                          aFTemplate);
}

void FunctionHook::HookProtectedMode()
{
  // Legacy code.  Uses the nsWindowsDLLInterceptor directly instead of
  // using the FunctionHook
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Plugin);
  WindowsDllInterceptor k32Intercept("kernel32.dll");
  k32Intercept.AddHook("CreateFileW",
                       reinterpret_cast<intptr_t>(CreateFileWHookFn),
                       (void**) &sCreateFileWStub);
  k32Intercept.AddHook("CreateFileA",
                       reinterpret_cast<intptr_t>(CreateFileAHookFn),
                       (void**) &sCreateFileAStub);
}

#endif // defined(XP_WIN)

#define FUN_HOOK(x) static_cast<FunctionHook*>(x)

void
FunctionHook::AddFunctionHooks(FunctionHookArray& aHooks)
{
}

#undef FUN_HOOK

} // namespace plugins
} // namespace mozilla
