/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#define MOZ_USE_LAUNCHER_ERROR

#include "freestanding/SafeThreadLocal.h"

#include "mozilla/NativeNt.h"
#include "nsWindowsHelpers.h"

#include <process.h>
#include <stdio.h>

// Need a non-inline function to bypass compiler optimization that the thread
// local storage pointer is cached in a register before accessing a thread-local
// variable.
MOZ_NEVER_INLINE PVOID SwapThreadLocalStoragePointer(PVOID aNewValue) {
  auto oldValue = mozilla::nt::RtlGetThreadLocalStoragePointer();
  mozilla::nt::RtlSetThreadLocalStoragePointerForTestingOnly(aNewValue);
  return oldValue;
}

static mozilla::freestanding::SafeThreadLocal<int*> gTheStorage;

static unsigned int __stdcall TestNonMainThread(void* aArg) {
  for (int i = 0; i < 100; ++i) {
    gTheStorage.set(&i);
    if (gTheStorage.get() != &i) {
      printf(
          "TEST-FAILED | TestSafeThreadLocal | "
          "A value is not correctly stored in the thread-local storage.\n");
      return 1;
    }
  }
  return 0;
}

extern "C" int wmain(int argc, wchar_t* argv[]) {
  int dummy = 0x1234;

  auto origHead = SwapThreadLocalStoragePointer(nullptr);
  // Setting gTheStorage when TLS is null.
  gTheStorage.set(&dummy);
  SwapThreadLocalStoragePointer(origHead);

  nsAutoHandle handles[8];
  for (auto& handle : handles) {
    handle.own(reinterpret_cast<HANDLE>(
        ::_beginthreadex(nullptr, 0, TestNonMainThread, nullptr, 0, nullptr)));
  }

  for (int i = 0; i < 100; ++i) {
    if (gTheStorage.get() != &dummy) {
      printf(
          "TEST-FAILED | TestSafeThreadLocal | "
          "A value is not correctly stored in the global scope.\n");
      return 1;
    }
  }

  for (auto& handle : handles) {
    ::WaitForSingleObject(handle, INFINITE);

    DWORD exitCode;
    if (!::GetExitCodeThread(handle, &exitCode) || exitCode) {
      return 1;
    }
  }

  return 0;
}
