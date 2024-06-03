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

class GetUiaClientPidsWin11 {
 public:
  static void Run(nsTArray<DWORD>& aPids);

 private:
  struct HandleAndPid {
    explicit HandleAndPid(HANDLE aHandle) : mHandle(aHandle) {}
    HANDLE mHandle;
    ULONG mPid = 0;
  };
  // Some local testing showed that we get around 40 handles when Firefox has
  // been started with a few tabs open for ~30 seconds before starting a UIA
  // client. That might increase with a longer duration, more tabs, etc., so
  // allow for some extra.
  using HandlesAndPids = AutoTArray<HandleAndPid, 128>;

  struct ThreadData {
    explicit ThreadData(HandlesAndPids& aHandlesAndPids)
        : mHandlesAndPids(aHandlesAndPids) {}
    HandlesAndPids& mHandlesAndPids;
    // Keeps track of the current index in mHandlesAndPids that is being
    // queried. When the thread is (re)started, it starts querying from this
    // index.
    size_t mCurrentIndex = 0;
  };

  static DWORD WINAPI QueryThreadProc(LPVOID aParameter) {
    // WARNING! Because this thread may be terminated unexpectedly due to a
    // hang, it must not do anything which acquires resources, allocates memory,
    // non-atomically modifies state, etc. It may not get a chance to clean up.
    auto& data = *(ThreadData*)aParameter;
    for (; data.mCurrentIndex < data.mHandlesAndPids.Length();
         ++data.mCurrentIndex) {
      auto& entry = data.mHandlesAndPids[data.mCurrentIndex];
      // Counter-intuitively, for UIA pipes, we're the client and the remote
      // process is the server.
      ::GetNamedPipeServerProcessId(entry.mHandle, &entry.mPid);
    }
    return 0;
  };
};

void GetUiaClientPidsWin11::Run(nsTArray<DWORD>& aPids) {
  // 1. Get all handles of interest in our process.
  HandlesAndPids handlesAndPids;
  const DWORD ourPid = ::GetCurrentProcessId();
  FindHandle([&](auto aInfo, auto aHandle) {
    // UIA pipes always have granted access 0x0012019F. Pipes with this access
    // can still hang, but this at least narrows down the handles we need to
    // check.
    if (aInfo.mPid == ourPid && aInfo.mGrantedAccess == 0x0012019F) {
      handlesAndPids.AppendElement(HandleAndPid(aHandle));
    }
    return true;
  });

  // 2. UIA creates a named pipe between the client and server processes. We
  // want to find our handle to those pipes (if any). For all named pipes, get
  // the process id of the remote end. We must use a background thread to query
  // pipes because this can hang on some pipes and there's no way to prevent
  // this other than terminating the thread. See bug 1899211 for more details.
  ThreadData threadData(handlesAndPids);
  while (threadData.mCurrentIndex < handlesAndPids.Length()) {
    // We use CreateThread here rather than Gecko's threading support because
    // we may terminate this thread and we must be certain it hasn't acquired
    // any resources which need to be cleaned up.
    nsAutoHandle thread(::CreateThread(nullptr, 0, QueryThreadProc,
                                       (LPVOID)&threadData, 0, nullptr));
    if (!thread) {
      return;
    }
    if (::WaitForSingleObject(thread, 50) == WAIT_OBJECT_0) {
      // We're done querying the handles.
      MOZ_ASSERT(threadData.mCurrentIndex == handlesAndPids.Length());
      break;
    }
    // The thread hung. Terminate it.
    ::TerminateThread(thread, 1);
    // The thread probably hung on threadData.mCurrentIndex, so skip this
    // handle. In the next iteration of this loop, we'll create another thread
    // and resume from that point. This could result in us skipping a handle if
    // the thread didn't actually hang, but took too long and was terminated
    // after incrementing but before querying the handle. At worst, we might
    // miss a UIA client in this case, but this should be very rare and it's an
    // acceptable compromise to avoid a main thread hang.
    ++threadData.mCurrentIndex;
  }

  // 3. Now that we have pids for all named pipes, get the name of those handles
  // and check whether they are UIA pipes. We can't do this in the thread above
  // because it allocates memory and that might not get cleaned up if the thread
  // is terminated.
  for (auto& entry : handlesAndPids) {
    if (!entry.mPid) {
      continue;  // Not a named pipe.
    }
    ULONG objNameBufLen;
    NTSTATUS ntStatus = ::NtQueryObject(
        entry.mHandle, (OBJECT_INFORMATION_CLASS)ObjectNameInformation, nullptr,
        0, &objNameBufLen);
    if (ntStatus != STATUS_INFO_LENGTH_MISMATCH) {
      continue;
    }
    auto objNameBuf = MakeUnique<std::byte[]>(objNameBufLen);
    ntStatus = ::NtQueryObject(entry.mHandle,
                               (OBJECT_INFORMATION_CLASS)ObjectNameInformation,
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
    if (StringBeginsWith(objName, u"\\Device\\NamedPipe\\UIA_PIPE_"_ns)) {
      aPids.AppendElement(entry.mPid);
    }
  }
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
    GetUiaClientPidsWin11::Run(aPids);
  } else {
    if (DWORD pid = GetUiaClientPidWin10()) {
      aPids.AppendElement(pid);
    }
  }
}

}  // namespace a11y
}  // namespace mozilla
