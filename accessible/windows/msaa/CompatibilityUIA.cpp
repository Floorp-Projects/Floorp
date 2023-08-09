/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Compatibility.h"

#include "mozilla/a11y/Platform.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/UniquePtr.h"
#include "nsReadableUtils.h"
#include "nsString.h"

#include "NtUndoc.h"

using namespace mozilla;

namespace mozilla {
namespace a11y {

void Compatibility::GetUiaClientPids(nsTArray<DWORD>& aPids) {
  if (!::GetModuleHandleW(L"uiautomationcore.dll")) {
    // UIAutomationCore isn't loaded, so there is no UIA client.
    return;
  }
  Telemetry::AutoTimer<Telemetry::A11Y_UIA_DETECTION_TIMING_MS> timer;

  NTSTATUS ntStatus;

  // First we must query for a list of all the open handles in the system.
  UniquePtr<std::byte[]> handleInfoBuf;
  ULONG handleInfoBufLen = sizeof(SYSTEM_HANDLE_INFORMATION_EX) +
                           1024 * sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX);

  // We must query for handle information in a loop, since we are effectively
  // asking the kernel to take a snapshot of all the handles on the system;
  // the size of the required buffer may fluctuate between successive calls.
  while (true) {
    // These allocations can be hundreds of megabytes on some computers, so
    // we should use fallible new here.
    handleInfoBuf = MakeUniqueFallible<std::byte[]>(handleInfoBufLen);
    if (!handleInfoBuf) {
      return;
    }

    ntStatus = ::NtQuerySystemInformation(
        (SYSTEM_INFORMATION_CLASS)SystemExtendedHandleInformation,
        handleInfoBuf.get(), handleInfoBufLen, &handleInfoBufLen);
    if (ntStatus == STATUS_INFO_LENGTH_MISMATCH) {
      continue;
    }

    if (!NT_SUCCESS(ntStatus)) {
      return;
    }

    break;
  }

  const DWORD ourPid = ::GetCurrentProcessId();
  auto handleInfo =
      reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(handleInfoBuf.get());
  for (ULONG index = 0; index < handleInfo->mHandleCount; ++index) {
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX& curHandle = handleInfo->mHandles[index];
    if (curHandle.mPid != ourPid) {
      // We're only interested in handles in our own process.
      continue;
    }
    HANDLE handle = reinterpret_cast<HANDLE>(curHandle.mHandle);

    // Get the name of the handle.
    ULONG objNameBufLen;
    ntStatus =
        ::NtQueryObject(handle, (OBJECT_INFORMATION_CLASS)ObjectNameInformation,
                        nullptr, 0, &objNameBufLen);
    if (ntStatus != STATUS_INFO_LENGTH_MISMATCH) {
      continue;
    }
    auto objNameBuf = MakeUnique<std::byte[]>(objNameBufLen);
    ntStatus =
        ::NtQueryObject(handle, (OBJECT_INFORMATION_CLASS)ObjectNameInformation,
                        objNameBuf.get(), objNameBufLen, &objNameBufLen);
    if (!NT_SUCCESS(ntStatus)) {
      continue;
    }
    auto objNameInfo =
        reinterpret_cast<OBJECT_NAME_INFORMATION*>(objNameBuf.get());
    if (!objNameInfo->Name.Length) {
      continue;
    }
    nsDependentString objName(objNameInfo->Name.Buffer,
                              objNameInfo->Name.Length / sizeof(wchar_t));

    // UIA creates a named pipe between the client and server processes. Find
    // our handle to that pipe (if any).
    if (StringBeginsWith(objName, u"\\Device\\NamedPipe\\UIA_PIPE_"_ns)) {
      // Get the process id of the remote end. Counter-intuitively, for this
      // pipe, we're the client and the remote process is the server.
      ULONG pid = 0;
      ::GetNamedPipeServerProcessId(handle, &pid);
      aPids.AppendElement(pid);
    }
  }
}

}  // namespace a11y
}  // namespace mozilla
