/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IOThreadChild.h"

#include "ContentProcess.h"
#include "base/shared_memory.h"
#include "mozilla/Preferences.h"
#include "mozilla/recordreplay/ParentIPC.h"

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include <stdlib.h>
#  include "mozilla/Sandbox.h"
#endif

#if (defined(XP_WIN) || defined(XP_MACOSX)) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#  include "nsAppDirectoryServiceDefs.h"
#  include "nsDirectoryService.h"
#  include "nsDirectoryServiceDefs.h"
#endif

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace dom {

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
static void SetTmpEnvironmentVariable(nsIFile* aValue) {
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

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
static void SetUpSandboxEnvironment() {
  MOZ_ASSERT(
      nsDirectoryService::gService,
      "SetUpSandboxEnvironment relies on nsDirectoryService being initialized");

  // On Windows, a sandbox-writable temp directory is used whenever the sandbox
  // is enabled.
  if (!IsContentSandboxEnabled()) {
    return;
  }

  nsCOMPtr<nsIFile> sandboxedContentTemp;
  nsresult rv = nsDirectoryService::gService->Get(
      NS_APP_CONTENT_PROCESS_TEMP_DIR, NS_GET_IID(nsIFile),
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

bool ContentProcess::Init(int aArgc, char* aArgv[]) {
  Maybe<uint64_t> childID;
  Maybe<bool> isForBrowser;
  Maybe<const char*> parentBuildID;
  char* prefsHandle = nullptr;
  char* prefMapHandle = nullptr;
  char* prefsLen = nullptr;
  char* prefMapSize = nullptr;
#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  nsCOMPtr<nsIFile> profileDir;
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
      prefsHandle = aArgv[i];
    } else if (strcmp(aArgv[i], "-prefMapHandle") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefMapHandle = aArgv[i];
#endif

    } else if (strcmp(aArgv[i], "-prefsLen") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefsLen = aArgv[i];
    } else if (strcmp(aArgv[i], "-prefMapSize") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefMapSize = aArgv[i];
    } else if (strcmp(aArgv[i], "-safeMode") == 0) {
      gSafeMode = true;

    } else if (strcmp(aArgv[i], "-parentBuildID") == 0) {
      if (++i == aArgc) {
        return false;
      }
      parentBuildID = Some(aArgv[i]);

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
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
#endif /* XP_MACOSX && MOZ_SANDBOX */
    }
  }

  // Did we find all the mandatory flags?
  if (childID.isNothing() || isForBrowser.isNothing() ||
      parentBuildID.isNothing()) {
    return false;
  }

  SharedPreferenceDeserializer deserializer;
  if (!deserializer.DeserializeFromSharedMemory(prefsHandle, prefMapHandle,
                                                prefsLen, prefMapSize)) {
    return false;
  }

  if (recordreplay::IsMiddleman()) {
    recordreplay::parent::InitializeMiddleman(aArgc, aArgv, ParentPid(),
                                              deserializer.GetPrefsHandle(),
                                              deserializer.GetPrefMapHandle());
  }

  mContent.Init(IOThreadChild::message_loop(), ParentPid(), *parentBuildID,
                IOThreadChild::channel(), *childID, *isForBrowser);

  mXREEmbed.Start();
#if (defined(XP_MACOSX)) && defined(MOZ_SANDBOX)
  mContent.SetProfileDir(profileDir);
#  if defined(DEBUG)
  // For WebReplay middleman processes, the sandbox is
  // started after receiving the SetProcessSandbox message.
  if (IsContentSandboxEnabled() &&
      Preferences::GetBool("security.sandbox.content.mac.earlyinit") &&
      !recordreplay::IsMiddleman()) {
    AssertMacSandboxEnabled();
  }
#  endif /* DEBUG */
#endif   /* XP_MACOSX && MOZ_SANDBOX */

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  SetUpSandboxEnvironment();
#endif

  return true;
}

// Note: CleanUp() never gets called in non-debug builds because we exit early
// in ContentChild::ActorDestroy().
void ContentProcess::CleanUp() { mXREEmbed.Stop(); }

}  // namespace dom
}  // namespace mozilla
