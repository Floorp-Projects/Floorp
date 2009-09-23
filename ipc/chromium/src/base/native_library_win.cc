// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <windows.h>

#include "base/file_path.h"
#include "base/path_service.h"

namespace base {

// static
NativeLibrary LoadNativeLibrary(const FilePath& library_path) {
  // Switch the current directory to the library directory as the library
  // may have dependencies on DLLs in this directory.
  bool restore_directory = false;
  std::wstring current_directory;
  if (PathService::Get(base::DIR_CURRENT, &current_directory)) {
    FilePath plugin_path = library_path.DirName();
    if (!plugin_path.value().empty()) {
      PathService::SetCurrentDirectory(plugin_path.value());
      restore_directory = true;
    }
  }

  HMODULE module = LoadLibrary(library_path.value().c_str());
  if (restore_directory)
    PathService::SetCurrentDirectory(current_directory);

  return module;
}

// static
void UnloadNativeLibrary(NativeLibrary library) {
  FreeLibrary(library);
}

// static
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          NativeLibraryFunctionNameType name) {
  return GetProcAddress(library, name);
}

}  // namespace base
