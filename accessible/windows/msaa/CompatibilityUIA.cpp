/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Compatibility.h"

#include "mozilla/ScopeExit.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/WindowsVersion.h"
#include "nspr/prenv.h"

#include "nsTHashMap.h"
#include "nsTHashSet.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsUnicharUtils.h"
#include "nsWinUtils.h"

#include "NtUndoc.h"

#if defined(UIA_LOGGING)

#  define LOG_ERROR(FuncName)                                       \
    {                                                               \
      DWORD err = ::GetLastError();                                 \
      nsPrintfCString msg(#FuncName " failed with code %u\n", err); \
      ::OutputDebugStringA(msg.get());                              \
    }

#else

#  define LOG_ERROR(FuncName)

#endif  // defined(UIA_LOGGING)

struct ByteArrayDeleter {
  void operator()(void* aBuf) { delete[] reinterpret_cast<char*>(aBuf); }
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
  path.AppendPrintf("\\Sessions\\%u\\BaseNamedObjects", sessionId);

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
      new char[objDirInfoBufLen]));

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
          new char[returnedLen]));
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

static const char* gBlockedUiaClients[] = {"osk.exe"};

static bool ShouldBlockUIAClient(nsIFile* aClientExe) {
  if (PR_GetEnv("MOZ_DISABLE_ACCESSIBLE_BLOCKLIST")) {
    return false;
  }

  nsAutoString leafName;
  nsresult rv = aClientExe->GetLeafName(leafName);
  if (NS_FAILED(rv)) {
    return false;
  }

  for (size_t index = 0, len = ArrayLength(gBlockedUiaClients); index < len;
       ++index) {
    if (leafName.EqualsIgnoreCase(gBlockedUiaClients[index])) {
      return true;
    }
  }

  return false;
}

namespace mozilla {
namespace a11y {

Maybe<DWORD> Compatibility::sUiaRemotePid;

Maybe<bool> Compatibility::OnUIAMessage(WPARAM aWParam, LPARAM aLParam) {
  auto clearUiaRemotePid = MakeScopeExit([]() { sUiaRemotePid = Nothing(); });

  Telemetry::AutoTimer<Telemetry::A11Y_UIA_DETECTION_TIMING_MS> timer;

  // UIA creates a section containing the substring "HOOK_SHMEM_"
  constexpr auto kStrHookShmem = u"HOOK_SHMEM_"_ns;

  // The section name always ends with this suffix, which is derived from the
  // current thread id and the UIA message's WPARAM and LPARAM.
  nsAutoString partialSectionSuffix;
  partialSectionSuffix.AppendPrintf("_%08x_%08x_%08x", ::GetCurrentThreadId(),
                                    static_cast<DWORD>(aLParam), aWParam);

  // Find any named Section that matches the naming convention of the UIA shared
  // memory.
  nsAutoHandle section;
  auto comparator = [&](const nsDependentSubstring& aName,
                        const nsDependentSubstring& aType) -> bool {
    if (aType.Equals(u"Section"_ns) && FindInReadable(kStrHookShmem, aName) &&
        StringEndsWith(aName, partialSectionSuffix)) {
      section.own(::OpenFileMapping(GENERIC_READ, FALSE,
                                    PromiseFlatString(aName).get()));
      return false;
    }

    return true;
  };

  if (!FindNamedObject(comparator) || !section) {
    return Nothing();
  }

  NTSTATUS ntStatus;

  // First we must query for a list of all the open handles in the system.
  UniquePtr<char[]> handleInfoBuf;
  ULONG handleInfoBufLen = sizeof(SYSTEM_HANDLE_INFORMATION_EX) +
                           1024 * sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX);

  // We must query for handle information in a loop, since we are effectively
  // asking the kernel to take a snapshot of all the handles on the system;
  // the size of the required buffer may fluctuate between successive calls.
  while (true) {
    // These allocations can be hundreds of megabytes on some computers, so
    // we should use fallible new here.
    handleInfoBuf = MakeUniqueFallible<char[]>(handleInfoBufLen);
    if (!handleInfoBuf) {
      return Nothing();
    }

    ntStatus = ::NtQuerySystemInformation(
        (SYSTEM_INFORMATION_CLASS)SystemExtendedHandleInformation,
        handleInfoBuf.get(), handleInfoBufLen, &handleInfoBufLen);
    if (ntStatus == STATUS_INFO_LENGTH_MISMATCH) {
      continue;
    }

    if (!NT_SUCCESS(ntStatus)) {
      return Nothing();
    }

    break;
  }

  const DWORD ourPid = ::GetCurrentProcessId();
  Maybe<PVOID> kernelObject;
  static Maybe<USHORT> sectionObjTypeIndex;
  nsTHashSet<uint32_t> nonSectionObjTypes;
  nsTHashMap<nsVoidPtrHashKey, DWORD> objMap;

  auto handleInfo =
      reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(handleInfoBuf.get());

  for (ULONG index = 0; index < handleInfo->mHandleCount; ++index) {
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX& curHandle = handleInfo->mHandles[index];

    HANDLE handle = reinterpret_cast<HANDLE>(curHandle.mHandle);

    // The mapping of the curHandle.mObjectTypeIndex field depends on the
    // underlying OS kernel. As we scan through the handle list, we record the
    // type indices such that we may use those values to skip over handles that
    // refer to non-section objects.
    if (sectionObjTypeIndex) {
      // If we know the type index for Sections, that's the fastest check...
      if (sectionObjTypeIndex.value() != curHandle.mObjectTypeIndex) {
        // Not a section
        continue;
      }
    } else if (nonSectionObjTypes.Contains(
                   static_cast<uint32_t>(curHandle.mObjectTypeIndex))) {
      // Otherwise we check whether or not the object type is definitely _not_
      // a Section...
      continue;
    } else if (ourPid == curHandle.mPid) {
      // Otherwise we need to issue some system calls to find out the object
      // type corresponding to the current handle's type index.
      ULONG objTypeBufLen;
      ntStatus = ::NtQueryObject(handle, ObjectTypeInformation, nullptr, 0,
                                 &objTypeBufLen);
      if (ntStatus != STATUS_INFO_LENGTH_MISMATCH) {
        continue;
      }

      auto objTypeBuf = MakeUnique<char[]>(objTypeBufLen);
      ntStatus =
          ::NtQueryObject(handle, ObjectTypeInformation, objTypeBuf.get(),
                          objTypeBufLen, &objTypeBufLen);
      if (!NT_SUCCESS(ntStatus)) {
        continue;
      }

      auto objType =
          reinterpret_cast<PUBLIC_OBJECT_TYPE_INFORMATION*>(objTypeBuf.get());

      // Now we check whether the object's type name matches "Section"
      nsDependentSubstring objTypeName(
          objType->TypeName.Buffer, objType->TypeName.Length / sizeof(wchar_t));
      if (!objTypeName.Equals(u"Section"_ns)) {
        nonSectionObjTypes.Insert(
            static_cast<uint32_t>(curHandle.mObjectTypeIndex));
        continue;
      }

      sectionObjTypeIndex = Some(curHandle.mObjectTypeIndex);
    }

    // At this point we know that curHandle references a Section object.
    // Now we can do some actual tests on it.

    if (ourPid != curHandle.mPid) {
      if (kernelObject && kernelObject.value() == curHandle.mObject) {
        // The kernel objects match -- we have found the remote pid!
        sUiaRemotePid = Some(curHandle.mPid);
        break;
      }

      // An object that is not ours. Since we do not yet know which kernel
      // object we're interested in, we'll save the current object for later.
      objMap.InsertOrUpdate(curHandle.mObject, curHandle.mPid);
    } else if (handle == section.get()) {
      // This is the file mapping that we opened above. We save this mObject
      // in order to compare to Section objects opened by other processes.
      kernelObject = Some(curHandle.mObject);
    }
  }

  if (!kernelObject) {
    return Nothing();
  }

  if (!sUiaRemotePid) {
    // We found kernelObject *after* we saw the remote process's copy. Now we
    // must look it up in objMap.
    DWORD pid;
    if (objMap.Get(kernelObject.value(), &pid)) {
      sUiaRemotePid = Some(pid);
    }
  }

  if (!sUiaRemotePid) {
    return Nothing();
  }

  a11y::SetInstantiator(sUiaRemotePid.value());

  // Block if necessary
  nsCOMPtr<nsIFile> instantiator;
  if (a11y::GetInstantiator(getter_AddRefs(instantiator)) &&
      ShouldBlockUIAClient(instantiator)) {
    return Some(false);
  }

  return Some(true);
}

}  // namespace a11y
}  // namespace mozilla
