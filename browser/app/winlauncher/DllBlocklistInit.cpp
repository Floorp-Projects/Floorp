/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "nsWindowsDllInterceptor.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/ImportDir.h"
#include "mozilla/NativeNt.h"
#include "mozilla/Types.h"
#include "mozilla/WindowsDllBlocklist.h"
#include "mozilla/WinHeaderOnlyUtils.h"

#include "DllBlocklistInit.h"
#include "freestanding/DllBlocklist.h"

#if defined(_MSC_VER)
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

namespace mozilla {

#if defined(MOZ_ASAN) || defined(_M_ARM64)

// This DLL blocking code is incompatible with ASAN because
// it is able to execute before ASAN itself has even initialized.
// Also, AArch64 has not been tested with this.
LauncherVoidResultWithLineInfo InitializeDllBlocklistOOP(
    const wchar_t* aFullImagePath, HANDLE aChildProcess) {
  return mozilla::Ok();
}

#else

LauncherVoidResultWithLineInfo InitializeDllBlocklistOOP(
    const wchar_t* aFullImagePath, HANDLE aChildProcess) {
  CrossProcessDllInterceptor intcpt(aChildProcess);
  intcpt.Init(L"ntdll.dll");

  bool ok = freestanding::stub_NtMapViewOfSection.SetDetour(
      aChildProcess, intcpt, "NtMapViewOfSection",
      &freestanding::patched_NtMapViewOfSection);
  if (!ok) {
    return LAUNCHER_ERROR_GENERIC();
  }

  ok = freestanding::stub_LdrLoadDll.SetDetour(
      aChildProcess, intcpt, "LdrLoadDll", &freestanding::patched_LdrLoadDll);
  if (!ok) {
    return LAUNCHER_ERROR_GENERIC();
  }

  // Because aChildProcess has just been created in a suspended state, its
  // dynamic linker has not yet been initialized, thus its executable has
  // not yet been linked with ntdll.dll. If the blocklist hook intercepts a
  // library load prior to the link, the hook will be unable to invoke any
  // ntdll.dll functions.
  //
  // We know that the executable for our *current* process's binary is already
  // linked into ntdll, so we obtain the IAT from our own executable and graft
  // it onto the child process's IAT, thus enabling the child process's hook to
  // safely make its ntdll calls.

  HMODULE ourModule;
#  if defined(_MSC_VER)
  ourModule = reinterpret_cast<HMODULE>(&__ImageBase);
#  else
  ourModule = ::GetModuleHandleW(nullptr);
#  endif  // defined(_MSC_VER)

  mozilla::nt::PEHeaders ourExeImage(ourModule);
  if (!ourExeImage) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
  }

  // As part of our mitigation of binary tampering, copy our import directory
  // from the original in our executable file.
  LauncherVoidResult importDirRestored = RestoreImportDirectory(
      aFullImagePath, ourExeImage, aChildProcess, ourModule);
  if (importDirRestored.isErr()) {
    return importDirRestored;
  }

  Maybe<Span<IMAGE_THUNK_DATA> > ntdllThunks =
      ourExeImage.GetIATThunksForModule("ntdll.dll");
  if (!ntdllThunks) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_INVALID_DATA);
  }

  SIZE_T bytesWritten;

  {  // Scope for prot
    PIMAGE_THUNK_DATA firstIatThunk = ntdllThunks.value().data();
    SIZE_T iatLength = ntdllThunks.value().LengthBytes();

    AutoVirtualProtect prot(firstIatThunk, iatLength, PAGE_READWRITE,
                            aChildProcess);
    if (!prot) {
      return LAUNCHER_ERROR_FROM_MOZ_WINDOWS_ERROR(prot.GetError());
    }

    ok = !!::WriteProcessMemory(aChildProcess, firstIatThunk, firstIatThunk,
                                iatLength, &bytesWritten);
    if (!ok || bytesWritten != iatLength) {
      return LAUNCHER_ERROR_FROM_LAST();
    }
  }

  // Tell the mozglue blocklist that we have bootstrapped
  uint32_t newFlags = eDllBlocklistInitFlagWasBootstrapped;

  if (gBlocklistInitFlags & eDllBlocklistInitFlagWasBootstrapped) {
    // If we ourselves were bootstrapped, then we are starting a child process
    // and need to set the appropriate flag.
    newFlags |= eDllBlocklistInitFlagIsChildProcess;
  }

  ok = !!::WriteProcessMemory(aChildProcess, &gBlocklistInitFlags, &newFlags,
                              sizeof(newFlags), &bytesWritten);
  if (!ok || bytesWritten != sizeof(newFlags)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  return Ok();
}

#endif  // defined(MOZ_ASAN) || defined(_M_ARM64)

}  // namespace mozilla
