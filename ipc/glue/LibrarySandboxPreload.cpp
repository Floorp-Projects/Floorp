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

nsAutoCString GetSandboxedGraphitePath() {
  nsCOMPtr<nsIFile> binaryPath;
  nsresult rv = mozilla::BinaryPath::GetFile(getter_AddRefs(binaryPath));
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get binary file\n");
  }

  nsCOMPtr<nsIFile> graphiteFile;
  rv = binaryPath->GetParent(getter_AddRefs(graphiteFile));
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get binary folder\n");
  }

  rv = graphiteFile->AppendNative(NS_LITERAL_CSTRING("libgraphitewasm.so"));
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get libgraphite file");
  }

  nsAutoString path;
  rv = graphiteFile->GetPath(path);
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Library preload failure: Failed to get libgraphite path\n");
  }

  nsAutoCString converted_path = NS_ConvertUTF16toUTF8(path);
  return converted_path;
}

void PreloadSandboxedDynamicLibraries() {
#ifdef MOZ_WASM_SANDBOXING_GRAPHITE
  nsAutoCString path = GetSandboxedGraphitePath();
  PRLibSpec libSpec;
  libSpec.type = PR_LibSpec_Pathname;
  libSpec.value.pathname = path.get();
  PRLibrary* ret = PR_LoadLibraryWithFlags(libSpec, PR_LD_LAZY);
  if (!ret) {
    MOZ_CRASH("Library preload failure: Failed to load libgraphite\n");
  }
#endif
}

}  // namespace ipc
}  // namespace mozilla