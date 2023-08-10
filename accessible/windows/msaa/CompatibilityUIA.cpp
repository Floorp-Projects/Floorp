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
#include "mozilla/WindowsVersion.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsTHashSet.h"
#include "nsWindowsHelpers.h"

#include "NtUndoc.h"

using namespace mozilla;

struct ByteArrayDeleter {
  void operator()(void* aBuf) { delete[] reinterpret_cast<std::byte*>(aBuf); }
};

typedef UniquePtr<OBJECT_DIRECTORY_INFORMATION, ByteArrayDeleter> ObjDirInfoPtr;

// ComparatorFnT returns true to continue searching, or else false to indicate
// search completion.
template <typename ComparatorFnT>
static bool FindNamedObject(const ComparatorFnT& aComparator) {
  // We want to enumerate every named kernel object in our session. We do this
  // by opening a directory object using a path constructed using the session
  // id under which our process resides.
  DWORD sessionId;
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &sessionId)) {
    return false;
  }

  nsAutoString path;
  path.AppendPrintf("\\Sessions\\%lu\\BaseNamedObjects", sessionId);

  UNICODE_STRING baseNamedObjectsName;
  ::RtlInitUnicodeString(&baseNamedObjectsName, path.get());

  OBJECT_ATTRIBUTES attributes;
  InitializeObjectAttributes(&attributes, &baseNamedObjectsName, 0, nullptr,
                             nullptr);

  HANDLE rawBaseNamedObjects;
  NTSTATUS ntStatus = ::NtOpenDirectoryObject(
      &rawBaseNamedObjects, DIRECTORY_QUERY | DIRECTORY_TRAVERSE, &attributes);
  if (!NT_SUCCESS(ntStatus)) {
    return false;
  }

  nsAutoHandle baseNamedObjects(rawBaseNamedObjects);

  ULONG context = 0, returnedLen;

  ULONG objDirInfoBufLen = 1024 * sizeof(OBJECT_DIRECTORY_INFORMATION);
  ObjDirInfoPtr objDirInfo(reinterpret_cast<OBJECT_DIRECTORY_INFORMATION*>(
      new std::byte[objDirInfoBufLen]));

  // Now query that directory object for every named object that it contains.

  BOOL firstCall = TRUE;

  do {
    ntStatus = ::NtQueryDirectoryObject(baseNamedObjects, objDirInfo.get(),
                                        objDirInfoBufLen, FALSE, firstCall,
                                        &context, &returnedLen);
#if defined(HAVE_64BIT_BUILD)
    if (!NT_SUCCESS(ntStatus)) {
      return false;
    }
#else
    if (ntStatus == STATUS_BUFFER_TOO_SMALL) {
      // This case only occurs on 32-bit builds running atop WOW64.
      // (See https://bugzilla.mozilla.org/show_bug.cgi?id=1423999#c3)
      objDirInfo.reset(reinterpret_cast<OBJECT_DIRECTORY_INFORMATION*>(
          new std::byte[returnedLen]));
      objDirInfoBufLen = returnedLen;
      continue;
    } else if (!NT_SUCCESS(ntStatus)) {
      return false;
    }
#endif

    // NtQueryDirectoryObject gave us an array of OBJECT_DIRECTORY_INFORMATION
    // structures whose final entry is zeroed out.
    OBJECT_DIRECTORY_INFORMATION* curDir = objDirInfo.get();
    while (curDir->mName.Length && curDir->mTypeName.Length) {
      // We use nsDependentSubstring here because UNICODE_STRINGs are not
      // guaranteed to be null-terminated.
      nsDependentSubstring objName(curDir->mName.Buffer,
                                   curDir->mName.Length / sizeof(wchar_t));
      nsDependentSubstring typeName(curDir->mTypeName.Buffer,
                                    curDir->mTypeName.Length / sizeof(wchar_t));

      if (!aComparator(objName, typeName)) {
        return true;
      }

      ++curDir;
    }

    firstCall = FALSE;
  } while (ntStatus == STATUS_MORE_ENTRIES);

  return false;
}

// ComparatorFnT returns true to continue searching, or else false to indicate
// search completion.
template <typename ComparatorFnT>
static bool FindHandle(const ComparatorFnT& aComparator) {
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
      return false;
    }
    ntStatus = ::NtQuerySystemInformation(
        (SYSTEM_INFORMATION_CLASS)SystemExtendedHandleInformation,
        handleInfoBuf.get(), handleInfoBufLen, &handleInfoBufLen);
    if (ntStatus == STATUS_INFO_LENGTH_MISMATCH) {
      continue;
    }
    if (!NT_SUCCESS(ntStatus)) {
      return false;
    }
    break;
  }

  auto handleInfo =
      reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(handleInfoBuf.get());
  for (ULONG index = 0; index < handleInfo->mHandleCount; ++index) {
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX& info = handleInfo->mHandles[index];
    HANDLE handle = reinterpret_cast<HANDLE>(info.mHandle);
    if (!aComparator(info, handle)) {
      return true;
    }
  }
  return false;
}

static void GetUiaClientPidsWin11(nsTArray<DWORD>& aPids) {
  const DWORD ourPid = ::GetCurrentProcessId();
  FindHandle([&](auto aInfo, auto aHandle) {
    if (aInfo.mPid != ourPid) {
      // We're only interested in handles in our own process.
      return true;
    }
    // Get the name of the handle.
    ULONG objNameBufLen;
    NTSTATUS ntStatus = ::NtQueryObject(
        aHandle, (OBJECT_INFORMATION_CLASS)ObjectNameInformation, nullptr, 0,
        &objNameBufLen);
    if (ntStatus != STATUS_INFO_LENGTH_MISMATCH) {
      return true;
    }
    auto objNameBuf = MakeUnique<std::byte[]>(objNameBufLen);
    ntStatus = ::NtQueryObject(aHandle,
                               (OBJECT_INFORMATION_CLASS)ObjectNameInformation,
                               objNameBuf.get(), objNameBufLen, &objNameBufLen);
    if (!NT_SUCCESS(ntStatus)) {
      return true;
    }
    auto objNameInfo =
        reinterpret_cast<OBJECT_NAME_INFORMATION*>(objNameBuf.get());
    if (!objNameInfo->Name.Length) {
      return true;
    }
    nsDependentString objName(objNameInfo->Name.Buffer,
                              objNameInfo->Name.Length / sizeof(wchar_t));

    // UIA creates a named pipe between the client and server processes. Find
    // our handle to that pipe (if any).
    if (StringBeginsWith(objName, u"\\Device\\NamedPipe\\UIA_PIPE_"_ns)) {
      // Get the process id of the remote end. Counter-intuitively, for this
      // pipe, we're the client and the remote process is the server.
      ULONG pid = 0;
      ::GetNamedPipeServerProcessId(aHandle, &pid);
      aPids.AppendElement(pid);
    }
    return true;
  });
}

static DWORD GetUiaClientPidWin10() {
  // UIA creates a section of the form "HOOK_SHMEM_%08lx_%08lx_%08lx_%08lx"
  constexpr auto kStrHookShmem = u"HOOK_SHMEM_"_ns;
  // The second %08lx is the thread id.
  nsAutoString sectionThread;
  sectionThread.AppendPrintf("_%08lx_", ::GetCurrentThreadId());
  // This is the number of characters from the end of the section name where
  // the sectionThread substring begins.
  constexpr size_t sectionThreadRPos = 27;
  // This is the length of sectionThread.
  constexpr size_t sectionThreadLen = 10;
  // Find any named Section that matches the naming convention of the UIA shared
  // memory. There can only be one of these at a time, since this only exists
  // while UIA is processing a request and it can only process a single request
  // on a single thread.
  nsAutoHandle section;
  auto objectComparator = [&](const nsDependentSubstring& aName,
                              const nsDependentSubstring& aType) -> bool {
    if (aType.Equals(u"Section"_ns) && FindInReadable(kStrHookShmem, aName) &&
        Substring(aName, aName.Length() - sectionThreadRPos,
                  sectionThreadLen) == sectionThread) {
      // Get a handle to this section so we can get its kernel object and
      // use that to find the handle for this section in the remote process.
      section.own(::OpenFileMapping(GENERIC_READ, FALSE,
                                    PromiseFlatString(aName).get()));
      return false;
    }
    return true;
  };
  if (!FindNamedObject(objectComparator) || !section) {
    return 0;
  }

  // Now, find the kernel object associated with our section, the handle in the
  // remote process associated with that kernel object and thus the remote
  // process id.
  NTSTATUS ntStatus;
  const DWORD ourPid = ::GetCurrentProcessId();
  Maybe<PVOID> kernelObject;
  static Maybe<USHORT> sectionObjTypeIndex;
  nsTHashSet<uint32_t> nonSectionObjTypes;
  nsTHashMap<nsVoidPtrHashKey, DWORD> objMap;
  DWORD remotePid = 0;
  FindHandle([&](auto aInfo, auto aHandle) {
    // The mapping of the aInfo.mObjectTypeIndex field depends on the
    // underlying OS kernel. As we scan through the handle list, we record the
    // type indices such that we may use those values to skip over handles that
    // refer to non-section objects.
    if (sectionObjTypeIndex) {
      // If we know the type index for Sections, that's the fastest check...
      if (sectionObjTypeIndex.value() != aInfo.mObjectTypeIndex) {
        // Not a section
        return true;
      }
    } else if (nonSectionObjTypes.Contains(
                   static_cast<uint32_t>(aInfo.mObjectTypeIndex))) {
      // Otherwise we check whether or not the object type is definitely _not_
      // a Section...
      return true;
    } else if (ourPid == aInfo.mPid) {
      // Otherwise we need to issue some system calls to find out the object
      // type corresponding to the current handle's type index.
      ULONG objTypeBufLen;
      ntStatus = ::NtQueryObject(aHandle, ObjectTypeInformation, nullptr, 0,
                                 &objTypeBufLen);
      if (ntStatus != STATUS_INFO_LENGTH_MISMATCH) {
        return true;
      }
      auto objTypeBuf = MakeUnique<std::byte[]>(objTypeBufLen);
      ntStatus =
          ::NtQueryObject(aHandle, ObjectTypeInformation, objTypeBuf.get(),
                          objTypeBufLen, &objTypeBufLen);
      if (!NT_SUCCESS(ntStatus)) {
        return true;
      }
      auto objType =
          reinterpret_cast<PUBLIC_OBJECT_TYPE_INFORMATION*>(objTypeBuf.get());
      // Now we check whether the object's type name matches "Section"
      nsDependentSubstring objTypeName(
          objType->TypeName.Buffer, objType->TypeName.Length / sizeof(wchar_t));
      if (!objTypeName.Equals(u"Section"_ns)) {
        nonSectionObjTypes.Insert(
            static_cast<uint32_t>(aInfo.mObjectTypeIndex));
        return true;
      }
      sectionObjTypeIndex = Some(aInfo.mObjectTypeIndex);
    }

    // At this point we know that aInfo references a Section object.
    // Now we can do some actual tests on it.
    if (ourPid != aInfo.mPid) {
      if (kernelObject && kernelObject.value() == aInfo.mObject) {
        // The kernel objects match -- we have found the remote pid!
        remotePid = aInfo.mPid;
        return false;
      }
      // An object that is not ours. Since we do not yet know which kernel
      // object we're interested in, we'll save the current object for later.
      objMap.InsertOrUpdate(aInfo.mObject, aInfo.mPid);
    } else if (aHandle == section.get()) {
      // This is the file mapping that we opened above. We save this mObject
      // in order to compare to Section objects opened by other processes.
      kernelObject = Some(aInfo.mObject);
    }
    return true;
  });

  if (remotePid) {
    return remotePid;
  }
  if (!kernelObject) {
    return 0;
  }

  // If we reach here, we found kernelObject *after* we saw the remote process's
  // copy. Now we must look it up in objMap.
  if (objMap.Get(kernelObject.value(), &remotePid)) {
    return remotePid;
  }

  return 0;
}

namespace mozilla {
namespace a11y {

void Compatibility::GetUiaClientPids(nsTArray<DWORD>& aPids) {
  if (!::GetModuleHandleW(L"uiautomationcore.dll")) {
    // UIAutomationCore isn't loaded, so there is no UIA client.
    return;
  }
  Telemetry::AutoTimer<Telemetry::A11Y_UIA_DETECTION_TIMING_MS> timer;
  if (IsWin11OrLater()) {
    GetUiaClientPidsWin11(aPids);
  } else {
    if (DWORD pid = GetUiaClientPidWin10()) {
      aPids.AppendElement(pid);
    }
  }
}

}  // namespace a11y
}  // namespace mozilla
