// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NATIVE_LIBRARY_H_
#define BASE_NATIVE_LIBRARY_H_

// This file defines a cross-platform "NativeLibrary" type which represents
// a loadable module.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#import <Carbon/Carbon.h>
#endif  // OS_*

class FilePath;

namespace base {

#if defined(OS_WIN)
typedef HMODULE NativeLibrary;
typedef char* NativeLibraryFunctionNameType;
#elif defined(OS_MACOSX)
typedef CFBundleRef NativeLibrary;
typedef CFStringRef NativeLibraryFunctionNameType;
#elif defined(OS_LINUX)
typedef void* NativeLibrary;
typedef const char* NativeLibraryFunctionNameType;
#endif  // OS_*

// Loads a native library from disk.  Release it with UnloadNativeLibrary when
// you're done.
NativeLibrary LoadNativeLibrary(const FilePath& library_path);

// Unloads a native library.
void UnloadNativeLibrary(NativeLibrary library);

// Gets a function pointer from a native library.
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          NativeLibraryFunctionNameType name);

}  // namespace base

#endif  // BASE_NATIVE_LIBRARY_H_
