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

static nsAutoCString GetSandboxedPath(const nsACString& libName) {
  nsCOMPtr<nsIFile> binaryPath;
  nsresult rv = mozilla::BinaryPath::GetFile(getter_AddRefs(binaryPath));
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get binary file\n");
  }

  nsCOMPtr<nsIFile> libFile;
  rv = binaryPath->GetParent(getter_AddRefs(libFile));
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get binary folder\n");
  }

  rv = libFile->AppendNative(libName);

  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get library file\n");
  }

  nsAutoString fullPath;
  rv = libFile->GetPath(fullPath);
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get library path\n");
  }

  nsAutoCString converted_path = NS_ConvertUTF16toUTF8(fullPath);
  return converted_path;
}

nsAutoCString GetSandboxedGraphitePath() {
  return GetSandboxedPath(
      NS_LITERAL_CSTRING(MOZ_DLL_PREFIX "graphitewasm" MOZ_DLL_SUFFIX));
}

nsAutoCString GetSandboxedOggPath() {
  return GetSandboxedPath(
      NS_LITERAL_CSTRING(MOZ_DLL_PREFIX "oggwasm" MOZ_DLL_SUFFIX));
}

PRLibrary* PreloadLibrary(const nsAutoCString& path) {
  PRLibSpec libSpec;
  libSpec.type = PR_LibSpec_Pathname;
  libSpec.value.pathname = path.get();
  PRLibrary* ret = PR_LoadLibraryWithFlags(libSpec, PR_LD_LAZY);
  return ret;
}

void PreloadSandboxedDynamicLibraries() {
  // The process level sandbox does not allow loading of dynamic libraries.
  // This preloads wasm sandboxed libraries before the process level sandbox is
  // enabled. Currently, this is only needed for Linux as Mac allows loading
  // libraries from the package file.
#if defined(XP_LINUX)
#  if defined(MOZ_WASM_SANDBOXING_GRAPHITE)
  if (!PreloadLibrary(GetSandboxedGraphitePath())) {
    MOZ_CRASH("Library preload failure: Failed to load libgraphite\n");
  }
#  endif
#  if defined(MOZ_WASM_SANDBOXING_OGG)
  if (!PreloadLibrary(GetSandboxedOggPath())) {
    MOZ_CRASH("Library preload failure: Failed to load libogg\n");
  }
#  endif
#endif
}

}  // namespace ipc
}  // namespace mozilla