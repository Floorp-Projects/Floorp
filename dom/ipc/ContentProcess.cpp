/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IOThreadChild.h"

#include "ContentProcess.h"
#include "base/shared_memory.h"
#include "mozilla/Preferences.h"
#include "mozilla/Scheduler.h"

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
#include <stdlib.h>
#endif

#if (defined(XP_WIN) || defined(XP_MACOSX)) && defined(MOZ_CONTENT_SANDBOX)
#include "mozilla/SandboxSettings.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#endif

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace dom {

#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
static void
SetTmpEnvironmentVariable(nsIFile* aValue)
{
  // Save the TMP environment variable so that is is picked up by GetTempPath().
  // Note that we specifically write to the TMP variable, as that is the first
  // variable that is checked by GetTempPath() to determine its output.
  nsAutoString fullTmpPath;
  nsresult rv = aValue->GetPath(fullTmpPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  Unused << NS_WARN_IF(!SetEnvironmentVariableW(L"TMP", fullTmpPath.get()));
  // We also set TEMP in case there is naughty third-party code that is
  // referencing the environment variable directly.
  Unused << NS_WARN_IF(!SetEnvironmentVariableW(L"TEMP", fullTmpPath.get()));
}
#endif


#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
static void
SetUpSandboxEnvironment()
{
  MOZ_ASSERT(nsDirectoryService::gService,
    "SetUpSandboxEnvironment relies on nsDirectoryService being initialized");

  // On Windows, a sandbox-writable temp directory is used whenever the sandbox
  // is enabled.
  if (!IsContentSandboxEnabled()) {
    return;
  }

  nsCOMPtr<nsIFile> sandboxedContentTemp;
  nsresult rv =
    nsDirectoryService::gService->Get(NS_APP_CONTENT_PROCESS_TEMP_DIR,
                                      NS_GET_IID(nsIFile),
                                      getter_AddRefs(sandboxedContentTemp));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Change the gecko defined temp directory to our sandbox-writable one.
  // Undefine returns a failure if the property is not already set.
  Unused << nsDirectoryService::gService->Undefine(NS_OS_TEMP_DIR);
  rv = nsDirectoryService::gService->Set(NS_OS_TEMP_DIR, sandboxedContentTemp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  SetTmpEnvironmentVariable(sandboxedContentTemp);
}
#endif

#ifdef ANDROID
static int gPrefsFd = -1;
static int gPrefMapFd = -1;

void
SetPrefsFd(int aFd)
{
  gPrefsFd = aFd;
}

void
SetPrefMapFd(int aFd)
{
  gPrefMapFd = aFd;
}
#endif

bool
ContentProcess::Init(int aArgc, char* aArgv[])
{
  Maybe<uint64_t> childID;
  Maybe<bool> isForBrowser;
  Maybe<base::SharedMemoryHandle> prefsHandle;
  Maybe<FileDescriptor> prefMapHandle;
  Maybe<size_t> prefsLen;
  Maybe<size_t> prefMapSize;
  Maybe<const char*> schedulerPrefs;
  Maybe<const char*> parentBuildID;
#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
  nsCOMPtr<nsIFile> profileDir;
#endif

  // Parses an arg containing a pointer-sized-integer.
  auto parseUIntPtrArg = [] (char*& aArg) {
    // ContentParent uses %zu to print a word-sized unsigned integer. So
    // even though strtoull() returns a long long int, it will fit in a
    // uintptr_t.
    return uintptr_t(strtoull(aArg, &aArg, 10));
  };

#ifdef XP_WIN
  auto parseHandleArg = [&] (char*& aArg) {
    return HANDLE(parseUIntPtrArg(aArg));
  };
#endif

  for (int i = 1; i < aArgc; i++) {
    if (!aArgv[i]) {
      continue;
    }

    if (strcmp(aArgv[i], "-appdir") == 0) {
      if (++i == aArgc) {
        return false;
      }
      nsDependentCString appDir(aArgv[i]);
      mXREEmbed.SetAppDir(appDir);

    } else if (strcmp(aArgv[i], "-childID") == 0) {
      if (++i == aArgc) {
        return false;
      }
      char* str = aArgv[i];
      childID = Some(strtoull(str, &str, 10));
      if (str[0] != '\0') {
        return false;
      }

    } else if (strcmp(aArgv[i], "-isForBrowser") == 0) {
      isForBrowser = Some(true);

    } else if (strcmp(aArgv[i], "-notForBrowser") == 0) {
      isForBrowser = Some(false);

#ifdef XP_WIN
    } else if (strcmp(aArgv[i], "-prefsHandle") == 0) {
      if (++i == aArgc) {
        return false;
      }
      char* str = aArgv[i];
      prefsHandle = Some(parseHandleArg(str));
      if (str[0] != '\0') {
        return false;
      }

    } else if (strcmp(aArgv[i], "-prefMapHandle") == 0) {
      if (++i == aArgc) {
        return false;
      }
      char* str = aArgv[i];
      // The FileDescriptor constructor will clone this handle when constructed,
      // so store it in a UniquePlatformHandle to make sure the original gets
      // closed.
      FileDescriptor::UniquePlatformHandle handle(parseHandleArg(str));
      prefMapHandle.emplace(handle.get());
      if (str[0] != '\0') {
        return false;
      }
#endif

    } else if (strcmp(aArgv[i], "-prefsLen") == 0) {
      if (++i == aArgc) {
        return false;
      }
      char* str = aArgv[i];
      prefsLen = Some(parseUIntPtrArg(str));
      if (str[0] != '\0') {
        return false;
      }

    } else if (strcmp(aArgv[i], "-prefMapSize") == 0) {
      if (++i == aArgc) {
        return false;
      }
      char* str = aArgv[i];
      prefMapSize = Some(parseUIntPtrArg(str));
      if (str[0] != '\0') {
        return false;
      }

    } else if (strcmp(aArgv[i], "-schedulerPrefs") == 0) {
      if (++i == aArgc) {
        return false;
      }
      schedulerPrefs = Some(aArgv[i]);

    } else if (strcmp(aArgv[i], "-safeMode") == 0) {
      gSafeMode = true;

    } else if (strcmp(aArgv[i], "-parentBuildID") == 0) {
      if (++i == aArgc) {
        return false;
      }
      parentBuildID = Some(aArgv[i]);

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
    } else if (strcmp(aArgv[i], "-profile") == 0) {
      if (++i == aArgc) {
        return false;
      }
      bool flag;
      nsresult rv = XRE_GetFileFromPath(aArgv[i], getter_AddRefs(profileDir));
      if (NS_FAILED(rv) || NS_FAILED(profileDir->Exists(&flag)) || !flag) {
        NS_WARNING("Invalid profile directory passed to content process.");
        profileDir = nullptr;
      }
#endif /* XP_MACOSX && MOZ_CONTENT_SANDBOX */
    }
  }

#ifdef ANDROID
  // Android is different; get the FD via gPrefsFd instead of a fixed fd.
  MOZ_RELEASE_ASSERT(gPrefsFd != -1);
  prefsHandle = Some(base::FileDescriptor(gPrefsFd, /* auto_close */ true));

  FileDescriptor::UniquePlatformHandle handle(gPrefMapFd);
  prefMapHandle.emplace(handle.get());
#elif XP_UNIX
  prefsHandle = Some(base::FileDescriptor(kPrefsFileDescriptor,
                                          /* auto_close */ true));

  // The FileDescriptor constructor will clone this handle when constructed,
  // so store it in a UniquePlatformHandle to make sure the original gets
  // closed.
  FileDescriptor::UniquePlatformHandle handle(kPrefMapFileDescriptor);
  prefMapHandle.emplace(handle.get());
#endif

  // Did we find all the mandatory flags?
  if (childID.isNothing() ||
      isForBrowser.isNothing() ||
      prefsHandle.isNothing() ||
      prefsLen.isNothing() ||
      prefMapHandle.isNothing() ||
      prefMapSize.isNothing() ||
      schedulerPrefs.isNothing() ||
      parentBuildID.isNothing()) {
    return false;
  }

  // Init the shared-memory base preference mapping first, so that only changed
  // preferences wind up in heap memory.
  Preferences::InitSnapshot(prefMapHandle.ref(), *prefMapSize);

  // Set up early prefs from the shared memory.
  base::SharedMemory shm;
  if (!shm.SetHandle(*prefsHandle, /* read_only */ true)) {
    NS_ERROR("failed to open shared memory in the child");
    return false;
  }
  if (!shm.Map(*prefsLen)) {
    NS_ERROR("failed to map shared memory in the child");
    return false;
  }
  Preferences::DeserializePreferences(static_cast<char*>(shm.memory()),
                                      *prefsLen);

  Scheduler::SetPrefs(*schedulerPrefs);
  mContent.Init(IOThreadChild::message_loop(),
                ParentPid(),
                *parentBuildID,
                IOThreadChild::channel(),
                *childID,
                *isForBrowser);
  mXREEmbed.Start();
#if (defined(XP_MACOSX)) && defined(MOZ_CONTENT_SANDBOX)
  mContent.SetProfileDir(profileDir);
#endif

#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
  SetUpSandboxEnvironment();
#endif

  return true;
}

// Note: CleanUp() never gets called in non-debug builds because we exit early
// in ContentChild::ActorDestroy().
void
ContentProcess::CleanUp()
{
  mXREEmbed.Stop();
}

} // namespace dom
} // namespace mozilla
