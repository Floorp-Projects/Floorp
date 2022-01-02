/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LibrarySandboxPreload.h"
#include "nsXPCOMPrivate.h"  // for XPCOM_DLL

#include "BinaryPath.h"
#include "prlink.h"

namespace mozilla {
namespace ipc {

PathString GetSandboxedRLBoxPath() {
  nsCOMPtr<nsIFile> libFile;
  nsresult rv = mozilla::BinaryPath::GetFile(getter_AddRefs(libFile));
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get binary file\n");
  }

  rv = libFile->SetNativeLeafName(XPCOM_DLL ""_ns);
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get library file\n");
  }

  return libFile->NativePath();
}

PRLibrary* PreloadLibrary(const PathString& path) {
  PRLibSpec libSpec;
#ifdef XP_WIN
  libSpec.type = PR_LibSpec_PathnameU;
  libSpec.value.pathname_u = path.get();
#else
  libSpec.type = PR_LibSpec_Pathname;
  libSpec.value.pathname = path.get();
#endif
  PRLibrary* ret = PR_LoadLibraryWithFlags(libSpec, PR_LD_LAZY);
  return ret;
}

}  // namespace ipc
}  // namespace mozilla
