// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_util.h"

#import <Cocoa/Cocoa.h>

#ifndef CHROMIUM_MOZILLA_BUILD
#include "app/l10n_util.h"
#endif
#include "base/file_path.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#ifndef CHROMIUM_MOZILLA_BUILD
#include "chrome/browser/cocoa/tab_window_controller.h"
#include "grit/generated_resources.h"
#endif

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
#ifdef CHROMIUM_MOZILLA_BUILD
  std::string str("Untitled");
  return string16(str.begin(), str.end());
#else
  NSString* title = nil;
  if ([[window delegate] isKindOfClass:[TabWindowController class]])
    title = [[window delegate] selectedTabTitle];
  else
    title = [window title];
  // If we don't yet have a title, use "Untitled".
  if (![title length])
    return WideToUTF16(l10n_util::GetString(
        IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED));

  return base::SysNSStringToUTF16(title);
#endif
}

bool IsWindowActive(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace platform_util
