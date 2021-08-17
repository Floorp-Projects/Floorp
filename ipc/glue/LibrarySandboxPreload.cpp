/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LibrarySandboxPreload.h"

#include "BinaryPath.h"
#include "prlink.h"

namespace mozilla {
namespace ipc {

nsCString GetSandboxedRLBoxPath() {
  nsCOMPtr<nsIFile> libFile;
  nsresult rv = mozilla::BinaryPath::GetFile(getter_AddRefs(libFile));
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get binary file\n");
  }

  rv = libFile->SetNativeLeafName(MOZ_DLL_PREFIX "rlbox" MOZ_DLL_SUFFIX ""_ns);
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get library file\n");
  }

  PathString fullPath = libFile->NativePath();
#ifdef XP_WIN
  return NS_ConvertUTF16toUTF8(fullPath);
#else
  return fullPath;
#endif
}

PRLibrary* PreloadLibrary(const nsCString& path) {
  PRLibSpec libSpec;
  libSpec.type = PR_LibSpec_Pathname;
  libSpec.value.pathname = path.get();
  PRLibrary* ret = PR_LoadLibraryWithFlags(libSpec, PR_LD_LAZY);
  return ret;
}

void PreloadSandboxedDynamicLibrary() {
  // The process level sandbox does not allow loading of dynamic libraries.
  // This preloads wasm sandboxed libraries before the process level sandbox is
  // enabled. Currently, this is only needed for Linux as Mac allows loading
  // libraries from the package file.
#if defined(XP_LINUX) && defined(MOZ_USING_WASM_SANDBOXING)
  if (!PreloadLibrary(GetSandboxedRLBoxPath())) {
    MOZ_CRASH("Library preload failure: Failed to load librlbox\n");
  }
#endif
}

}  // namespace ipc
}  // namespace mozilla
