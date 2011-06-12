/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 *
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steven Michaud <smichaud@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Use "dyld interposing" to hook methods imported from other libraries in the
// plugin child process.  The basic technique is described at
// http://books.google.com/books?id=K8vUkpOXhN4C&pg=PA73&lpg=PA73&dq=__interpose&source=bl&ots=OJnnXZYpZC&sig=o7I3lXvoduUi13SrPfOON7o3do4&hl=en&ei=AoehS9brCYGQNrvsmeUM&sa=X&oi=book_result&ct=result&resnum=6&ved=0CBsQ6AEwBQ#v=onepage&q=__interpose&f=false.
// The idea of doing it for the plugin child process comes from Chromium code,
// particularly from plugin_carbon_interpose_mac.cc
// (http://codesearch.google.com/codesearch/p?hl=en#OAMlx_jo-ck/src/chrome/browser/plugin_carbon_interpose_mac.cc&q=nscursor&exact_package=chromium&d=1&l=168)
// and from PluginProcessHost::Init() in plugin_process_host.cc
// (http://codesearch.google.com/codesearch/p?hl=en#OAMlx_jo-ck/src/content/browser/plugin_process_host.cc&q=nscursor&exact_package=chromium&d=1&l=222).

// These hooks are needed to make certain OS calls work from the child process
// (a background process) that would normally only work when called in the
// parent process (the foreground process).  They allow us to serialize
// information from the child process to the parent process, so that the same
// (or equivalent) calls can be made from the parent process.

// This file lives in a seperate module (libplugin_child_interpose.dylib),
// which will get loaded by the OS before any other modules when the plugin
// child process is launched (from GeckoChildProcessHost::
// PerformAsyncLaunchInternal()).  For this reason it shouldn't link in other
// browser modules when loaded.  Instead it should use dlsym() to load
// pointers to the methods it wants to call in other modules.

#if !defined(__LP64__)

#include <dlfcn.h>
#import <Carbon/Carbon.h>

// The header file QuickdrawAPI.h is missing on OS X 10.7 and up (though the
// QuickDraw APIs defined in it are still present) -- so we need to supply the
// relevant parts of its contents here.  It's likely that Apple will eventually
// remove the APIs themselves (probably in OS X 10.8), so we need to make them
// weak imports, and test for their presence before using them.
#if !defined(__QUICKDRAWAPI__)

typedef struct Cursor;
extern "C" void SetCursor(const Cursor * crsr) __attribute__((weak_import));

#endif /* __QUICKDRAWAPI__ */

BOOL (*OnSetThemeCursorPtr) (ThemeCursor) = NULL;
BOOL (*OnSetCursorPtr) (const Cursor*) = NULL;
BOOL (*OnHideCursorPtr) () = NULL;
BOOL (*OnShowCursorPtr) () = NULL;

static BOOL loadXULPtrs()
{
  if (!OnSetThemeCursorPtr) {
    // mac_plugin_interposing_child_OnSetThemeCursor(ThemeCursor cursor) is in
    // PluginInterposeOSX.mm
    OnSetThemeCursorPtr = (BOOL(*)(ThemeCursor))
      dlsym(RTLD_DEFAULT, "mac_plugin_interposing_child_OnSetThemeCursor");
  }
  if (!OnSetCursorPtr) {
    // mac_plugin_interposing_child_OnSetCursor(const Cursor* cursor) is in
    // PluginInterposeOSX.mm
    OnSetCursorPtr = (BOOL(*)(const Cursor*))
      dlsym(RTLD_DEFAULT, "mac_plugin_interposing_child_OnSetCursor");
  }
  if (!OnHideCursorPtr) {
    // mac_plugin_interposing_child_OnHideCursor() is in PluginInterposeOSX.mm
    OnHideCursorPtr = (BOOL(*)())
      dlsym(RTLD_DEFAULT, "mac_plugin_interposing_child_OnHideCursor");
  }
  if (!OnShowCursorPtr) {
    // mac_plugin_interposing_child_OnShowCursor() is in PluginInterposeOSX.mm
    OnShowCursorPtr = (BOOL(*)())
      dlsym(RTLD_DEFAULT, "mac_plugin_interposing_child_OnShowCursor");
  }
  return (OnSetCursorPtr && OnSetThemeCursorPtr && OnHideCursorPtr && OnShowCursorPtr);
}

static OSStatus MacPluginChildSetThemeCursor(ThemeCursor cursor)
{
  if (loadXULPtrs()) {
    OnSetThemeCursorPtr(cursor);
  }
  return ::SetThemeCursor(cursor);
}

static void MacPluginChildSetCursor(const Cursor* cursor)
{
  if (::SetCursor) {
    if (loadXULPtrs()) {
      OnSetCursorPtr(cursor);
    }
    ::SetCursor(cursor);
  }
}

static CGError MacPluginChildCGDisplayHideCursor(CGDirectDisplayID display)
{
  if (loadXULPtrs()) {
    OnHideCursorPtr();
  }
  return ::CGDisplayHideCursor(display);
}

static CGError MacPluginChildCGDisplayShowCursor(CGDirectDisplayID display)
{
  if (loadXULPtrs()) {
    OnShowCursorPtr();
  }
  return ::CGDisplayShowCursor(display);
}

#pragma mark -

struct interpose_substitution {
  const void* replacement;
  const void* original;
};

#define INTERPOSE_FUNCTION(function) \
    { reinterpret_cast<const void*>(MacPluginChild##function), \
      reinterpret_cast<const void*>(function) }

__attribute__((used)) static const interpose_substitution substitutions[]
    __attribute__((section("__DATA, __interpose"))) = {
  INTERPOSE_FUNCTION(SetThemeCursor),
  INTERPOSE_FUNCTION(CGDisplayHideCursor),
  INTERPOSE_FUNCTION(CGDisplayShowCursor),
  // SetCursor() and other QuickDraw APIs will probably be removed in OS X
  // 10.8.  But this will make 'SetCursor' NULL, which will just stop the OS
  // from interposing it (tested using an INTERPOSE_FUNCTION_BROKEN macro
  // that just sets the second address of each tuple to NULL).
  INTERPOSE_FUNCTION(SetCursor),
};

#endif  // !__LP64__
