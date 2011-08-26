// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_util.h"

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"

namespace platform_util {

void ShowItemInFolder(const FilePath& full_path) {
  NSString* path_string = base::SysUTF8ToNSString(full_path.value());
  [[NSWorkspace sharedWorkspace] selectFile:path_string
                   inFileViewerRootedAtPath:nil];
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  return [view window];
}

string16 GetWindowTitle(gfx::NativeWindow window) {
  std::string str("Untitled");
  return string16(str.begin(), str.end());
}

bool IsWindowActive(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace platform_util
