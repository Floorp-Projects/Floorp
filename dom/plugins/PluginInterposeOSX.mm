/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/basictypes.h"
#include "nsCocoaUtils.h"
#include "PluginModuleChild.h"
#include "nsDebug.h"
#include <set>
#import <AppKit/AppKit.h>
#import <objc/runtime.h>

using mozilla::plugins::PluginModuleChild;
using mozilla::plugins::AssertPluginThread;

namespace mac_plugin_interposing {
namespace parent {

// Tracks plugin windows currently visible.
std::set<uint32_t> plugin_visible_windows_set_;
// Tracks full screen windows currently visible.
std::set<uint32_t> plugin_fullscreen_windows_set_;
// Tracks modal windows currently visible.
std::set<uint32_t> plugin_modal_windows_set_;

void OnPluginShowWindow(uint32_t window_id,
                        CGRect window_bounds,
                        bool modal) {
  plugin_visible_windows_set_.insert(window_id);

  if (modal)
    plugin_modal_windows_set_.insert(window_id);

  CGRect main_display_bounds = ::CGDisplayBounds(CGMainDisplayID());

  if (CGRectEqualToRect(window_bounds, main_display_bounds) &&
      (plugin_fullscreen_windows_set_.find(window_id) ==
       plugin_fullscreen_windows_set_.end())) {
    plugin_fullscreen_windows_set_.insert(window_id);

    nsCocoaUtils::HideOSChromeOnScreen(TRUE, [[NSScreen screens] objectAtIndex:0]);
  }
}

static void ActivateProcess(pid_t pid) {
  ProcessSerialNumber process;
  OSStatus status = ::GetProcessForPID(pid, &process);

  if (status == noErr) {
    SetFrontProcess(&process);
  } else {
    NS_WARNING("Unable to get process for pid.");
  }
}

// Must be called on the UI thread.
// If plugin_pid is -1, the browser will be the active process on return,
// otherwise that process will be given focus back before this function returns.
static void ReleasePluginFullScreen(pid_t plugin_pid) {
  // Releasing full screen only works if we are the frontmost process; grab
  // focus, but give it back to the plugin process if requested.
  ActivateProcess(base::GetCurrentProcId());

  nsCocoaUtils::HideOSChromeOnScreen(FALSE, [[NSScreen screens] objectAtIndex:0]);

  if (plugin_pid != -1) {
    ActivateProcess(plugin_pid);
  }
}

void OnPluginHideWindow(uint32_t window_id, pid_t aPluginPid) {
  bool had_windows = !plugin_visible_windows_set_.empty();
  plugin_visible_windows_set_.erase(window_id);
  bool browser_needs_activation = had_windows &&
      plugin_visible_windows_set_.empty();

  plugin_modal_windows_set_.erase(window_id);
  if (plugin_fullscreen_windows_set_.find(window_id) !=
      plugin_fullscreen_windows_set_.end()) {
    plugin_fullscreen_windows_set_.erase(window_id);
    pid_t plugin_pid = browser_needs_activation ? -1 : aPluginPid;
    browser_needs_activation = false;
    ReleasePluginFullScreen(plugin_pid);
  }

  if (browser_needs_activation) {
    ActivateProcess(getpid());
  }
}

} // parent
} // namespace mac_plugin_interposing

namespace mac_plugin_interposing {
namespace child {

// TODO(stuartmorgan): Make this an IPC to order the plugin process above the
// browser process only if the browser is current frontmost.
void FocusPluginProcess() {
  ProcessSerialNumber this_process, front_process;
  if ((GetCurrentProcess(&this_process) != noErr) ||
      (GetFrontProcess(&front_process) != noErr)) {
    return;
  }

  Boolean matched = false;
  if ((SameProcess(&this_process, &front_process, &matched) == noErr) &&
      !matched) {
    SetFrontProcess(&this_process);
  }
}

void NotifyBrowserOfPluginShowWindow(uint32_t window_id, CGRect bounds,
                                     bool modal) {
  AssertPluginThread();

  PluginModuleChild *pmc = PluginModuleChild::current();
  if (pmc)
    pmc->PluginShowWindow(window_id, modal, bounds);
}

void NotifyBrowserOfPluginHideWindow(uint32_t window_id, CGRect bounds) {
  AssertPluginThread();

  PluginModuleChild *pmc = PluginModuleChild::current();
  if (pmc)
    pmc->PluginHideWindow(window_id);
}

struct WindowInfo {
  uint32_t window_id;
  CGRect bounds;
  WindowInfo(NSWindow* window) {
    NSInteger window_num = [window windowNumber];
    window_id = window_num > 0 ? window_num : 0;
    bounds = NSRectToCGRect([window frame]);
  }
};

static void OnPluginWindowClosed(const WindowInfo& window_info) {
  if (window_info.window_id == 0)
    return;
  mac_plugin_interposing::child::NotifyBrowserOfPluginHideWindow(window_info.window_id,
                                                                 window_info.bounds);
}

static void OnPluginWindowShown(const WindowInfo& window_info, BOOL is_modal) {
  // The window id is 0 if it has never been shown (including while it is the
  // process of being shown for the first time); when that happens, we'll catch
  // it in _setWindowNumber instead.
  static BOOL s_pending_display_is_modal = NO;
  if (window_info.window_id == 0) {
    if (is_modal)
      s_pending_display_is_modal = YES;
    return;
  }
  if (s_pending_display_is_modal) {
    is_modal = YES;
    s_pending_display_is_modal = NO;
  }
  mac_plugin_interposing::child::NotifyBrowserOfPluginShowWindow(
    window_info.window_id, window_info.bounds, is_modal);
}

} // child
} // namespace mac_plugin_interposing

using mac_plugin_interposing::child::WindowInfo;

@interface NSWindow (PluginInterposing)
- (void)pluginInterpose_orderOut:(id)sender;
- (void)pluginInterpose_orderFront:(id)sender;
- (void)pluginInterpose_makeKeyAndOrderFront:(id)sender;
- (void)pluginInterpose_setWindowNumber:(NSInteger)num;
@end

@implementation NSWindow (PluginInterposing)

- (void)pluginInterpose_orderOut:(id)sender {
  WindowInfo window_info(self);
  [self pluginInterpose_orderOut:sender];
  OnPluginWindowClosed(window_info);
}

- (void)pluginInterpose_orderFront:(id)sender {
  mac_plugin_interposing::child::FocusPluginProcess();
  [self pluginInterpose_orderFront:sender];
  OnPluginWindowShown(WindowInfo(self), NO);
}

- (void)pluginInterpose_makeKeyAndOrderFront:(id)sender {
  mac_plugin_interposing::child::FocusPluginProcess();
  [self pluginInterpose_makeKeyAndOrderFront:sender];
  OnPluginWindowShown(WindowInfo(self), NO);
}

- (void)pluginInterpose_setWindowNumber:(NSInteger)num {
  if (num > 0)
    mac_plugin_interposing::child::FocusPluginProcess();
  [self pluginInterpose_setWindowNumber:num];
  if (num > 0)
    OnPluginWindowShown(WindowInfo(self), NO);
}

@end

@interface NSApplication (PluginInterposing)
- (NSInteger)pluginInterpose_runModalForWindow:(NSWindow*)window;
@end

@implementation NSApplication (PluginInterposing)

- (NSInteger)pluginInterpose_runModalForWindow:(NSWindow*)window {
  mac_plugin_interposing::child::FocusPluginProcess();
  // This is out-of-order relative to the other calls, but runModalForWindow:
  // won't return until the window closes, and the order only matters for
  // full-screen windows.
  OnPluginWindowShown(WindowInfo(window), YES);
  return [self pluginInterpose_runModalForWindow:window];
}

@end

static void ExchangeMethods(Class target_class,
                            BOOL class_method,
                            SEL original,
                            SEL replacement) {
  Method m1;
  Method m2;
  if (class_method) {
    m1 = class_getClassMethod(target_class, original);
    m2 = class_getClassMethod(target_class, replacement);
  } else {
    m1 = class_getInstanceMethod(target_class, original);
    m2 = class_getInstanceMethod(target_class, replacement);
  }

  if (m1 == m2)
    return;

  if (m1 && m2)
    method_exchangeImplementations(m1, m2);
  else
    NS_NOTREACHED("Cocoa swizzling failed");
}

namespace mac_plugin_interposing {
namespace child {

void SetUpCocoaInterposing() {
  Class nswindow_class = [NSWindow class];
  ExchangeMethods(nswindow_class, NO, @selector(orderOut:),
                  @selector(pluginInterpose_orderOut:));
  ExchangeMethods(nswindow_class, NO, @selector(orderFront:),
                  @selector(pluginInterpose_orderFront:));
  ExchangeMethods(nswindow_class, NO, @selector(makeKeyAndOrderFront:),
                  @selector(pluginInterpose_makeKeyAndOrderFront:));
  ExchangeMethods(nswindow_class, NO, @selector(_setWindowNumber:),
                  @selector(pluginInterpose_setWindowNumber:));

  ExchangeMethods([NSApplication class], NO, @selector(runModalForWindow:),
                  @selector(pluginInterpose_runModalForWindow:));
}

}  // namespace child
}  // namespace mac_plugin_interposing

