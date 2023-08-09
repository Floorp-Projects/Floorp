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
#  include "mozilla/SandboxSettings.h"
#endif

#include "nsAppRunner.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/ProcessUtils.h"
#include "mozilla/GeckoArgs.h"
#include "mozilla/Omnijar.h"
#include "nsCategoryManagerUtils.h"

using mozilla::ipc::IOThreadChild;

namespace mozilla::dom {

static nsresult GetGREDir(nsIFile** aResult) {
  nsCOMPtr<nsIFile> current;
  nsresult rv = XRE_GetBinaryPath(getter_AddRefs(current));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef XP_DARWIN
  // Walk out of [subprocess].app/Contents/MacOS to the real GRE dir
  const int depth = 4;
#else
  const int depth = 1;
#endif

  for (int i = 0; i < depth; ++i) {
    nsCOMPtr<nsIFile> parent;
    rv = current->GetParent(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);

    current = parent;
    NS_ENSURE_TRUE(current, NS_ERROR_UNEXPECTED);
  }

#ifdef XP_DARWIN
  rv = current->SetNativeLeafName("Resources"_ns);
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  current.forget(aResult);

  return NS_OK;
}

ContentProcess::ContentProcess(ProcessId aParentPid,
                               const nsID& aMessageChannelId)
    : ProcessChild(aParentPid, aMessageChannelId) {
  NS_LogInit();
}

ContentProcess::~ContentProcess() { NS_LogTerm(); }

bool ContentProcess::Init(int aArgc, char* aArgv[]) {
  Maybe<uint64_t> childID = geckoargs::sChildID.Get(aArgc, aArgv);
  Maybe<bool> isForBrowser = Nothing();
  Maybe<const char*> parentBuildID =
      geckoargs::sParentBuildID.Get(aArgc, aArgv);
  Maybe<uint64_t> jsInitHandle;
  Maybe<uint64_t> jsInitLen = geckoargs::sJsInitLen.Get(aArgc, aArgv);

  nsCOMPtr<nsIFile> appDirArg;
  Maybe<const char*> appDir = geckoargs::sAppDir.Get(aArgc, aArgv);
  if (appDir.isSome()) {
    bool flag;
    nsresult rv = XRE_GetFileFromPath(*appDir, getter_AddRefs(appDirArg));
    if (NS_FAILED(rv) || NS_FAILED(appDirArg->Exists(&flag)) || !flag) {
      NS_WARNING("Invalid application directory passed to content process.");
      appDirArg = nullptr;
    }
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
  nsCOMPtr<nsIFile> profileDir;
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

  nsCOMPtr<nsIFile> greDir;
  nsresult rv = GetGREDir(getter_AddRefs(greDir));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIFile> xpcomAppDir = appDirArg ? appDirArg : greDir;

  rv = mDirProvider.Initialize(xpcomAppDir, greDir);
  if (NS_FAILED(rv)) {
    return false;
  }

  // Handle the -greomni/-appomni flags (unless the forkserver already
  // preloaded the jar(s)).
  if (!Omnijar::IsInitialized()) {
    Omnijar::ChildProcessInit(aArgc, aArgv);
  }

  rv = NS_InitXPCOM(nullptr, xpcomAppDir, &mDirProvider);
  if (NS_FAILED(rv)) {
    return false;
  }

  // "app-startup" is the name of both the category and the event
  NS_CreateServicesFromCategory("app-startup", nullptr, "app-startup", nullptr);

#if (defined(XP_MACOSX)) && defined(MOZ_SANDBOX)
  mContent.SetProfileDir(profileDir);
#  if defined(DEBUG)
  if (IsContentSandboxEnabled()) {
    AssertMacSandboxEnabled();
  }
#  endif /* DEBUG */
#endif   /* XP_MACOSX && MOZ_SANDBOX */

  // Do this as early as possible to get the parent process to initialize the
  // background thread since we'll likely need database information very soon.
  mozilla::ipc::BackgroundChild::Startup();
  mozilla::ipc::BackgroundChild::InitContentStarter(&mContent);

  return true;
}

// Note: CleanUp() never gets called in non-debug builds because we exit early
// in ContentChild::ActorDestroy().
void ContentProcess::CleanUp() {
  mDirProvider.DoShutdown();
  NS_ShutdownXPCOM(nullptr);
}

}  // namespace mozilla::dom
