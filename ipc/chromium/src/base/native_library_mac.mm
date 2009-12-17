// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#import <Carbon/Carbon.h>

#include "base/file_path.h"
#include "base/scoped_cftyperef.h"

namespace base {

// static
NativeLibrary LoadNativeLibrary(const FilePath& library_path) {
  scoped_cftyperef<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault,
      (const UInt8*)library_path.value().c_str(),
      library_path.value().length(),
      true));
  if (!url)
    return NULL;

  return CFBundleCreate(kCFAllocatorDefault, url.get());
}

// static
void UnloadNativeLibrary(NativeLibrary library) {
  CFRelease(library);
}

// static
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          NativeLibraryFunctionNameType name) {
  return CFBundleGetFunctionPointerForName(library, name);
}

}  // namespace base
