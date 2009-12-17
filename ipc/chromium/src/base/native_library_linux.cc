// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <dlfcn.h>

#include "base/file_path.h"
#include "base/logging.h"

namespace base {

// static
NativeLibrary LoadNativeLibrary(const FilePath& library_path) {
  void* dl = dlopen(library_path.value().c_str(), RTLD_LAZY);
  if (!dl)
    NOTREACHED() << "dlopen failed: " << dlerror();

  return dl;
}

// static
void UnloadNativeLibrary(NativeLibrary library) {
  int ret = dlclose(library);
  if (ret < 0)
    NOTREACHED() << "dlclose failed: " << dlerror();
}

// static
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          NativeLibraryFunctionNameType name) {
  return dlsym(library, name);
}

}  // namespace base
