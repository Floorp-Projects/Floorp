/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Compatibility.h"

#include "mozilla/Telemetry.h"

#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsWinUtils.h"

#include "NtUndoc.h"

#if defined(UIA_LOGGING)

#define LOG_ERROR(FuncName) \
  { \
    DWORD err = ::GetLastError(); \
    nsPrintfCString msg(#FuncName " failed with code %u\n", err); \
    ::OutputDebugStringA(msg.get()); \
  }

#else

#define LOG_ERROR(FuncName)

#endif // defined(UIA_LOGGING)

static bool
GetLocalObjectHandle(DWORD aSrcPid, HANDLE aSrcHandle, nsAutoHandle& aProcess,
                     nsAutoHandle& aLocal)
{
  aLocal.reset();

  if (!aProcess) {
    HANDLE rawProcess = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, aSrcPid);
    if (!rawProcess) {
      LOG_ERROR(OpenProcess);
      return false;
    }

    aProcess.own(rawProcess);
  }

  HANDLE rawDuped;
  if (!::DuplicateHandle(aProcess.get(), aSrcHandle, ::GetCurrentProcess(),
                         &rawDuped, GENERIC_READ, FALSE, 0)) {
    LOG_ERROR(DuplicateHandle);
    return false;
  }

  aLocal.own(rawDuped);

  return true;
}

namespace mozilla {
namespace a11y {

Maybe<DWORD> Compatibility::sUiaRemotePid;

Maybe<bool>
Compatibility::OnUIAMessage(WPARAM aWParam, LPARAM aLParam)
{
  Maybe<DWORD>& remotePid = sUiaRemotePid;
  auto clearUiaRemotePid = MakeScopeExit([&remotePid]() {
    remotePid = Nothing();
  });

  Telemetry::AutoTimer<Telemetry::A11Y_UIA_DETECTION_TIMING_MS> timer;

  static auto pNtQuerySystemInformation =
    reinterpret_cast<decltype(&::NtQuerySystemInformation)>(
      ::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"),
                       "NtQuerySystemInformation"));

  static auto pNtQueryObject =
    reinterpret_cast<decltype(&::NtQueryObject)>(
      ::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "NtQueryObject"));

  // UIA creates a section containing the substring "HOOK_SHMEM_"
  NS_NAMED_LITERAL_STRING(kStrHookShmem, "HOOK_SHMEM_");

  // The section name always ends with this suffix, which is derived from the
  // current thread id and the UIA message's WPARAM and LPARAM.
  nsAutoString partialSectionSuffix;
  partialSectionSuffix.AppendPrintf("_%08x_%08x_%08x", ::GetCurrentThreadId(),
                                    aLParam, aWParam);

  NTSTATUS ntStatus;

  // First we must query for a list of all the open handles in the system.
  UniquePtr<char[]> handleInfoBuf;
  ULONG handleInfoBufLen = sizeof(SYSTEM_HANDLE_INFORMATION_EX) +
                           1024 * sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX);

  // We must query for handle information in a loop, since we are effectively
  // asking the kernel to take a snapshot of all the handles on the system;
  // the size of the required buffer may fluctuate between successive calls.
  while (true) {
    handleInfoBuf = MakeUnique<char[]>(handleInfoBufLen);

    ntStatus = pNtQuerySystemInformation(
                 (SYSTEM_INFORMATION_CLASS) SystemExtendedHandleInformation,
                 handleInfoBuf.get(), handleInfoBufLen, &handleInfoBufLen);
    if (ntStatus == STATUS_INFO_LENGTH_MISMATCH) {
      continue;
    }

    if (!NT_SUCCESS(ntStatus)) {
      return Nothing();
    }

    break;
  }

  // Now we iterate through the system handle list, searching for a section
  // handle whose name matches the section name used by UIA.

  static Maybe<USHORT> sSectionObjTypeIndex;

  const DWORD ourPid = ::GetCurrentProcessId();

  Maybe<PVOID> kernelObject;

  ULONG lastPid = 0;
  nsAutoHandle process;

  auto handleInfo = reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(handleInfoBuf.get());

  for (ULONG index = 0; index < handleInfo->mHandleCount; ++index) {
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX& curHandle = handleInfo->mHandles[index];

    if (lastPid && lastPid == curHandle.mPid && !process) {
      // During the previous iteration, we could not obtain a handle for this
      // pid. Skip any remaining handles belonging to that pid.
      continue;
    }

    // As a perf optimization, we reuse the same process handle as long as we're
    // still looking at the same pid. Once the pid changes, we need to reset
    // process so that we open a new handle to the newly-seen process.
    if (lastPid != curHandle.mPid) {
      process.reset();
    }

    nsAutoHandle handle;

    if (kernelObject.isSome() && kernelObject.value() == curHandle.mObject) {
      // If we know the value of the underlying kernel object, we can immediately
      // check for equality by comparing against curHandle.mObject
      remotePid = Some(static_cast<DWORD>(curHandle.mPid));
      break;
    } else if (sSectionObjTypeIndex.isSome()) {
      // Otherwise, if we know which object type value corresponds to a Section
      // object, we can use that to eliminate any handles that are not sections.
      if (curHandle.mObjectTypeIndex != sSectionObjTypeIndex.value()) {
        // Not a section handle
        continue;
      }
    } else {
      // Otherwise we need to query the handle to determine its type. Note that
      // each handle in the system list is relative to its owning process, so
      // we cannot do anything with it until we duplicate the handle into our
      // own process.

      lastPid = curHandle.mPid;

      if (!GetLocalObjectHandle((DWORD) curHandle.mPid,
                                (HANDLE) curHandle.mHandle,
                                process, handle)) {
        // We don't have access to do this, assume this handle isn't relevant
        continue;
      }

      // Now we have our own handle to the object, lets find out what type of
      // object this handle represents. Any handle whose type is not "Section"
      // is of no interest to us.
      ULONG objTypeBufLen;
      ntStatus = pNtQueryObject(handle, ObjectTypeInformation, nullptr,
                                0, &objTypeBufLen);
      if (ntStatus != STATUS_INFO_LENGTH_MISMATCH) {
        continue;
      }

      auto objTypeBuf = MakeUnique<char[]>(objTypeBufLen);
      ntStatus = pNtQueryObject(handle, ObjectTypeInformation, objTypeBuf.get(),
                                objTypeBufLen, &objTypeBufLen);
      if (!NT_SUCCESS(ntStatus)) {
        // We don't have access to do this, assume this handle isn't relevant
        continue;
      }

      auto objType =
        reinterpret_cast<PUBLIC_OBJECT_TYPE_INFORMATION*>(objTypeBuf.get());

      nsDependentString objTypeName(objType->TypeName.Buffer,
                                    objType->TypeName.Length / sizeof(wchar_t));
      if (!objTypeName.Equals(NS_LITERAL_STRING("Section"))) {
        // Not a section, so we don't care about this handle anymore.
        continue;
      }

      // We have a section, save this handle's type code so that we can go
      // faster in future iterations.
      sSectionObjTypeIndex = Some(curHandle.mObjectTypeIndex);
    }

    // If we reached this point without needing to query the handle, then we
    // need to open it here so that we can query its name.
    lastPid = curHandle.mPid;

    if ((!process || !handle) &&
        !GetLocalObjectHandle((DWORD) curHandle.mPid, (HANDLE) curHandle.mHandle,
                              process, handle)) {
      // We don't have access to do this, assume this handle isn't relevant
      continue;
    }

    // At this point, |handle| is a valid section handle. Let's try to find
    // out the name of its underlying object.
    ULONG objNameBufLen;
    ntStatus = pNtQueryObject(handle,
                              (OBJECT_INFORMATION_CLASS)ObjectNameInformation,
                              nullptr, 0, &objNameBufLen);
    if (ntStatus != STATUS_INFO_LENGTH_MISMATCH) {
      continue;
    }

    auto objNameBuf = MakeUnique<char[]>(objNameBufLen);
    ntStatus = pNtQueryObject(handle,
                              (OBJECT_INFORMATION_CLASS)ObjectNameInformation,
                              objNameBuf.get(), objNameBufLen, &objNameBufLen);
    if (!NT_SUCCESS(ntStatus)) {
      continue;
    }

    auto objNameInfo = reinterpret_cast<OBJECT_NAME_INFORMATION*>(objNameBuf.get());
    if (!objNameInfo->mName.Length) {
      // This section is unnamed. We don't care about those.
      continue;
    }

    nsDependentString objName(objNameInfo->mName.Buffer,
                              objNameInfo->mName.Length / sizeof(wchar_t));

    // Check to see if the section's name matches our expected name.
    if (!FindInReadable(kStrHookShmem, objName) ||
        !StringEndsWith(objName, partialSectionSuffix)) {
      // The names don't match, continue searching.
      continue;
    }

    // At this point we have a section handle whose name matches the one that
    // we're looking for.

    if (curHandle.mPid == ourPid) {
      // Our own process also has a handle to the section of interest. While we
      // don't want our own pid, this *does* give us an opportunity to speed up
      // future iterations by examining each handle for its kernel object (which
      // is the same for all processes) instead of searching by name.
      kernelObject = Some(curHandle.mObject);
      continue;
    }

    // Bingo! We want this pid!
    remotePid = Some(static_cast<DWORD>(curHandle.mPid));

    break;
  }

  if (!remotePid) {
    return Nothing();
  }

  a11y::SetInstantiator(remotePid.value());

  /* This is where we could block UIA stuff
  nsCOMPtr<nsIFile> instantiator;
  if (a11y::GetInstantiator(getter_AddRefs(instantiator)) &&
      ShouldBlockUIAClient(instantiator)) {
    return Some(false);
  }
  */

  return Some(true);
}

} // namespace a11y
} // namespace mozilla
