/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IOThreadChild.h"

#include "ContentProcess.h"

#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
#include "mozilla/WindowsVersion.h"
#endif

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
  // for Vista or later with sandbox pref level >= 1.
  return (IsVistaOrLater() &&
    (Preferences::GetInt("security.sandbox.content.level") >= 1));
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
  NS_WARN_IF(!SetEnvironmentVariableW(L"TMP", fullTmpPath.get()));
  // We also set TEMP in case there is naughty third-party code that is
  // referencing the environment variable directly.
  NS_WARN_IF(!SetEnvironmentVariableW(L"TEMP", fullTmpPath.get()));
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
  NS_WARN_IF(setenv("TMPDIR", fullTmpPath.get(), 1) != 0);
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

void
ContentProcess::SetAppDir(const nsACString& aPath)
{
  mXREEmbed.SetAppDir(aPath);
}

bool
ContentProcess::Init()
{
    mContent.Init(IOThreadChild::message_loop(),
                  ParentPid(),
                  IOThreadChild::channel());
    mXREEmbed.Start();
    mContent.InitXPCOM();
    mContent.InitGraphicsDeviceData();

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
