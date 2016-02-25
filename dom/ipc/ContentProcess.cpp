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

#if (defined(XP_WIN) || defined(XP_MACOSX)) && defined(MOZ_CONTENT_SANDBOX)
#include "mozilla/Preferences.h"
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

static const char*
SandboxTempDirParent()
{
  // On Windows, the sandbox-writable temp directory resides in the
  // low integrity sandbox base directory.
  return NS_WIN_LOW_INTEGRITY_TEMP_BASE;
}
#endif

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
static bool
IsSandboxTempDirRequired()
{
  // On OSX, use the sandbox-writable temp when the pref level >= 1.
  return (Preferences::GetInt("security.sandbox.content.level") >= 1);
}

static const char*
SandboxTempDirParent()
{
  return NS_OS_TEMP_DIR;
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

  nsAdoptingString tempDirSuffix =
    Preferences::GetString("security.sandbox.content.tempDirSuffix");
  if (tempDirSuffix.IsEmpty()) {
    NS_WARNING("Sandbox-writable temp directory suffix pref not set.");
    return;
  }

  // Get the parent of our sandbox writable temp directory.
  nsCOMPtr<nsIFile> lowIntegrityTemp;
  nsresult rv = nsDirectoryService::gService->Get(SandboxTempDirParent(),
                                                  NS_GET_IID(nsIFile),
                                                  getter_AddRefs(lowIntegrityTemp));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Append our profile specific temp name.
  rv = lowIntegrityTemp->Append(NS_LITERAL_STRING("Temp-") + tempDirSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Change the gecko defined temp directory to our sandbox-writable one.
  // Undefine returns a failure if the property is not already set.
  Unused << nsDirectoryService::gService->Undefine(NS_OS_TEMP_DIR);
  rv = nsDirectoryService::gService->Set(NS_OS_TEMP_DIR, lowIntegrityTemp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
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
