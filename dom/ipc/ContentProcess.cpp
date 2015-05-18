/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IOThreadChild.h"

#include "ContentProcess.h"

#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
#include "mozilla/Preferences.h"
#include "mozilla/WindowsVersion.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#endif

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace dom {

#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
static void
SetUpSandboxEnvironment()
{
  MOZ_ASSERT(nsDirectoryService::gService,
    "SetUpSandboxEnvironment relies on nsDirectoryService being initialized");

  // A low integrity temp only currently makes sense for Vista or Later and
  // sandbox pref level 1.
  if (!IsVistaOrLater() ||
      Preferences::GetInt("security.sandbox.content.level") != 1) {
    return;
  }

  nsAdoptingString tempDirSuffix =
    Preferences::GetString("security.sandbox.content.tempDirSuffix");
  if (tempDirSuffix.IsEmpty()) {
    NS_WARNING("Low integrity temp suffix pref not set.");
    return;
  }

  // Get the base low integrity Mozilla temp directory.
  nsCOMPtr<nsIFile> lowIntegrityTemp;
  nsresult rv = nsDirectoryService::gService->Get(NS_WIN_LOW_INTEGRITY_TEMP_BASE,
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

  // Change the gecko defined temp directory to our low integrity one.
  // Undefine returns a failure if the property is not already set.
  unused << nsDirectoryService::gService->Undefine(NS_OS_TEMP_DIR);
  rv = nsDirectoryService::gService->Set(NS_OS_TEMP_DIR, lowIntegrityTemp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

#if defined(NIGHTLY_BUILD)
static void
CleanUpOldSandboxEnvironment()
{
  // Temporary code to clean up the old low integrity temp directories.
  // The removal of this is tracked by bug 1165818.
  nsCOMPtr<nsIFile> lowIntegrityMozilla;
  nsresult rv = NS_GetSpecialDirectory(NS_WIN_LOCAL_APPDATA_LOW_DIR,
                              getter_AddRefs(lowIntegrityMozilla));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = lowIntegrityMozilla->Append(NS_LITERAL_STRING(MOZ_USER_DIR));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> iter;
  rv = lowIntegrityMozilla->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  bool more;
  nsCOMPtr<nsISupports> elem;
  while (NS_SUCCEEDED(iter->HasMoreElements(&more)) && more) {
    rv = iter->GetNext(getter_AddRefs(elem));
    if (NS_FAILED(rv)) {
      break;
    }

    nsCOMPtr<nsIFile> file = do_QueryInterface(elem);
    if (!file) {
      continue;
    }

    nsAutoString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_FAILED(rv)) {
      continue;
    }

    if (leafName.Find(NS_LITERAL_STRING("MozTemp-{")) == 0) {
      file->Remove(/* aRecursive */ true);
    }
  }
}
#endif
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

#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
    SetUpSandboxEnvironment();
#endif
    
    return true;
}

void
ContentProcess::CleanUp()
{
#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX) && defined(NIGHTLY_BUILD)
    CleanUpOldSandboxEnvironment();
#endif
    mXREEmbed.Stop();
}

} // namespace dom
} // namespace mozilla
