// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PLATFORM_UTIL_H_
#define CHROME_COMMON_PLATFORM_UTIL_H_

#include "base/gfx/native_widget_types.h"
#include "base/string16.h"

class FilePath;

namespace platform_util {

// Show the given file in a file manager. If possible, select the file.
void ShowItemInFolder(const FilePath& full_path);

// Get the top level window for the native view. This can return NULL.
gfx::NativeWindow GetTopLevel(gfx::NativeView view);

// Get the title of the window.
string16 GetWindowTitle(gfx::NativeWindow window);

// Returns true if |window| is the foreground top level window.
bool IsWindowActive(gfx::NativeWindow window);

}

#endif  // CHROME_COMMON_PLATFORM_UTIL_H_
