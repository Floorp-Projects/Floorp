/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IOThreadChild.h"

#include "ContentProcess.h"
#include "ContentPrefs.h"

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
#include <stdlib.h>
#endif

#if (defined(XP_WIN) || defined(XP_MACOSX)) && defined(MOZ_CONTENT_SANDBOX)
#include "mozilla/Preferences.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#endif

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace dom {

#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
static bool
IsSandboxTempDirRequired()
{
  // On Windows, a sandbox-writable temp directory is only used
  // when sandbox pref level >= 1.
  return Preferences::GetInt("security.sandbox.content.level") >= 1;
}

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

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
static bool
IsSandboxTempDirRequired()
{
  // On OSX, use the sandbox-writable temp when the pref level >= 1.
  return (Preferences::GetInt("security.sandbox.content.level") >= 1);
}

static void
SetTmpEnvironmentVariable(nsIFile* aValue)
{
  nsAutoCString fullTmpPath;
  nsresult rv = aValue->GetNativePath(fullTmpPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  Unused << NS_WARN_IF(setenv("TMPDIR", fullTmpPath.get(), 1) != 0);
}
#endif

#if (defined(XP_WIN) || defined(XP_MACOSX)) && defined(MOZ_CONTENT_SANDBOX)
static void
SetUpSandboxEnvironment()
{
  MOZ_ASSERT(nsDirectoryService::gService,
    "SetUpSandboxEnvironment relies on nsDirectoryService being initialized");

  if (!IsSandboxTempDirRequired()) {
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

bool
ContentProcess::Init(int aArgc, char* aArgv[])
{
  // If passed in grab the application path for xpcom init
  bool foundAppdir = false;
  bool foundChildID = false;
  bool foundIsForBrowser = false;
  bool foundIntPrefs = false;
  bool foundBoolPrefs = false;
  bool foundStringPrefs = false;

  uint64_t childID;
  bool isForBrowser;

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
  // If passed in grab the profile path for sandboxing
  bool foundProfile = false;
  nsCOMPtr<nsIFile> profileDir;
#endif

  InfallibleTArray<PrefSetting> prefsArray;
  for (int idx = aArgc; idx > 0; idx--) {
    if (!aArgv[idx]) {
      continue;
    }

    if (!strcmp(aArgv[idx], "-appdir")) {
      MOZ_ASSERT(!foundAppdir);
      if (foundAppdir) {
        continue;
      }
      nsCString appDir;
      appDir.Assign(nsDependentCString(aArgv[idx+1]));
      mXREEmbed.SetAppDir(appDir);
      foundAppdir = true;
    } else if (!strcmp(aArgv[idx], "-childID")) {
      MOZ_ASSERT(!foundChildID);
      if (foundChildID) {
        continue;
      }
      if (idx + 1 < aArgc) {
        childID = strtoull(aArgv[idx + 1], nullptr, 10);
        foundChildID = true;
      }
    } else if (!strcmp(aArgv[idx], "-isForBrowser") || !strcmp(aArgv[idx], "-notForBrowser")) {
      MOZ_ASSERT(!foundIsForBrowser);
      if (foundIsForBrowser) {
        continue;
      }
      isForBrowser = strcmp(aArgv[idx], "-notForBrowser");
      foundIsForBrowser = true;
    } else if (!strcmp(aArgv[idx], "-intPrefs")) {
      SET_PREF_PHASE(BEGIN_INIT_PREFS);
      char* str = aArgv[idx + 1];
      while (*str) {
        int32_t index = strtol(str, &str, 10);
        str++;
        MaybePrefValue value(PrefValue(static_cast<int32_t>(strtol(str, &str, 10))));
        str++;
        PrefSetting pref(nsCString(ContentPrefs::GetContentPref(index)), value, MaybePrefValue());
        prefsArray.AppendElement(pref);
      }
      SET_PREF_PHASE(END_INIT_PREFS);
      foundIntPrefs = true;
    } else if (!strcmp(aArgv[idx], "-boolPrefs")) {
      SET_PREF_PHASE(BEGIN_INIT_PREFS);
      char* str = aArgv[idx + 1];
      while (*str) {
        int32_t index = strtol(str, &str, 10);
        str++;
        MaybePrefValue value(PrefValue(!!strtol(str, &str, 10)));
        str++;
        PrefSetting pref(nsCString(ContentPrefs::GetContentPref(index)), value, MaybePrefValue());
        prefsArray.AppendElement(pref);
      }
      SET_PREF_PHASE(END_INIT_PREFS);
      foundBoolPrefs = true;
    } else if (!strcmp(aArgv[idx], "-stringPrefs")) {
      SET_PREF_PHASE(BEGIN_INIT_PREFS);
      char* str = aArgv[idx + 1];
      while (*str) {
        int32_t index = strtol(str, &str, 10);
        str++;
        int32_t length = strtol(str, &str, 10);
        str++;
        MaybePrefValue value(PrefValue(nsCString(str, length)));
        PrefSetting pref(nsCString(ContentPrefs::GetContentPref(index)), value, MaybePrefValue());
        prefsArray.AppendElement(pref);
        str += length + 1;
      }
      SET_PREF_PHASE(END_INIT_PREFS);
      foundStringPrefs = true;
    }

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
    else if (!strcmp(aArgv[idx], "-profile")) {
      MOZ_ASSERT(!foundProfile);
      if (foundProfile) {
        continue;
      }
      bool flag;
      nsresult rv = XRE_GetFileFromPath(aArgv[idx+1], getter_AddRefs(profileDir));
      if (NS_FAILED(rv) ||
          NS_FAILED(profileDir->Exists(&flag)) || !flag) {
        NS_WARNING("Invalid profile directory passed to content process.");
        profileDir = nullptr;
      }
      foundProfile = true;
    }
#endif /* XP_MACOSX && MOZ_CONTENT_SANDBOX */

    bool allFound = foundAppdir && foundChildID && foundIsForBrowser && foundIntPrefs && foundBoolPrefs && foundStringPrefs;

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
    allFound &= foundProfile;
#endif

    if (allFound) {
      break;
    }
  }
  Preferences::SetInitPreferences(&prefsArray);
  mContent.Init(IOThreadChild::message_loop(),
                ParentPid(),
                IOThreadChild::channel(),
                childID,
                isForBrowser);
  mXREEmbed.Start();
#if (defined(XP_MACOSX)) && defined(MOZ_CONTENT_SANDBOX)
  mContent.SetProfileDir(profileDir);
#endif

#if (defined(XP_WIN) || defined(XP_MACOSX)) && defined(MOZ_CONTENT_SANDBOX)
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
