/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IOThreadChild.h"

#include "ContentProcess.h"
#include "base/shared_memory.h"
#include "mozilla/Preferences.h"

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include <stdlib.h>
#  include "mozilla/Sandbox.h"
#endif

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#  include "mozilla/WindowsProcessMitigations.h"
#endif

#if (defined(XP_WIN) || defined(XP_MACOSX)) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#  include "nsAppDirectoryServiceDefs.h"
#  include "nsDirectoryService.h"
#  include "nsDirectoryServiceDefs.h"
#endif

#include "nsAppRunner.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/ProcessUtils.h"
#include "mozilla/GeckoArgs.h"

using mozilla::ipc::IOThreadChild;

namespace mozilla::dom {

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
  // is enabled, except when win32k is locked down when we no longer require a
  // temp directory.
  if (!IsContentSandboxEnabled() || IsWin32kLockedDown()) {
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
  Maybe<uint64_t> childID = geckoargs::sChildID.Get(aArgc, aArgv);
  Maybe<bool> isForBrowser = Nothing();
  Maybe<const char*> parentBuildID =
      geckoargs::sParentBuildID.Get(aArgc, aArgv);
  Maybe<uint64_t> jsInitHandle;
  Maybe<uint64_t> jsInitLen = geckoargs::sJsInitLen.Get(aArgc, aArgv);
#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  nsCOMPtr<nsIFile> profileDir;
#endif

  Maybe<const char*> appDir = geckoargs::sAppDir.Get(aArgc, aArgv);
  if (appDir.isSome()) {
    mXREEmbed.SetAppDir(nsDependentCString(*appDir));
  }

  Maybe<bool> safeMode = geckoargs::sSafeMode.Get(aArgc, aArgv);
  if (safeMode.isSome()) {
    gSafeMode = *safeMode;
  }

  Maybe<bool> isForBrowerParam = geckoargs::sIsForBrowser.Get(aArgc, aArgv);
  Maybe<bool> notForBrowserParam = geckoargs::sNotForBrowser.Get(aArgc, aArgv);
  if (isForBrowerParam.isSome()) {
    isForBrowser = Some(true);
  }
  if (notForBrowserParam.isSome()) {
    isForBrowser = Some(false);
  }

  // command line: [-jsInitHandle handle] -jsInitLen length
#ifdef XP_WIN
  jsInitHandle = geckoargs::sJsInitHandle.Get(aArgc, aArgv);
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  bool flag;
  Maybe<const char*> profile = geckoargs::sProfile.Get(aArgc, aArgv);
  // xpcshell self-test on macOS will hit this, so check isSome() otherwise
  // Maybe<> assertions will MOZ_CRASH() us.
  if (profile.isSome()) {
    nsresult rv = XRE_GetFileFromPath(*profile, getter_AddRefs(profileDir));
    if (NS_FAILED(rv) || NS_FAILED(profileDir->Exists(&flag)) || !flag) {
      NS_WARNING("Invalid profile directory passed to content process.");
      profileDir = nullptr;
    }
  } else {
    NS_WARNING("No profile directory passed to content process.");
  }
#endif /* XP_MACOSX && MOZ_SANDBOX */

  // Did we find all the mandatory flags?
  if (childID.isNothing() || isForBrowser.isNothing() ||
      parentBuildID.isNothing()) {
    return false;
  }

  if (!ProcessChild::InitPrefs(aArgc, aArgv)) {
    return false;
  }

  if (!::mozilla::ipc::ImportSharedJSInit(jsInitHandle.valueOr(0),
                                          jsInitLen.valueOr(0))) {
    return false;
  }

  mContent.Init(TakeInitialEndpoint(), *parentBuildID, *childID, *isForBrowser);

  mXREEmbed.Start();
#if (defined(XP_MACOSX)) && defined(MOZ_SANDBOX)
  mContent.SetProfileDir(profileDir);
#  if defined(DEBUG)
  if (IsContentSandboxEnabled()) {
    AssertMacSandboxEnabled();
  }
#  endif /* DEBUG */
#endif   /* XP_MACOSX && MOZ_SANDBOX */

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  SetUpSandboxEnvironment();
#endif

  // Do this as early as possible to get the parent process to initialize the
  // background thread since we'll likely need database information very soon.
  mozilla::ipc::BackgroundChild::Startup();
  mozilla::ipc::BackgroundChild::InitContentStarter(&mContent);

  return true;
}

// Note: CleanUp() never gets called in non-debug builds because we exit early
// in ContentChild::ActorDestroy().
void ContentProcess::CleanUp() { mXREEmbed.Stop(); }

}  // namespace mozilla::dom
