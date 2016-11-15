/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginQuirks_h
#define dom_plugins_PluginQuirks_h

namespace mozilla {
namespace plugins {

// Quirks mode support for various plugin mime types
enum PluginQuirks {
  QUIRKS_NOT_INITIALIZED                          = 0,
  // Silverlight assumes it is transparent in windowless mode. This quirk
  // matches the logic in nsNPAPIPluginInstance::SetWindowless.
  QUIRK_SILVERLIGHT_DEFAULT_TRANSPARENT           = 1 << 0,
  // Win32: Hook TrackPopupMenu api so that we can swap out parent
  // hwnds. The api will fail with parents not associated with our
  // child ui thread. See WinlessHandleEvent for details.
  QUIRK_WINLESS_TRACKPOPUP_HOOK                   = 1 << 1,
  // Win32: Throttle flash WM_USER+1 heart beat messages to prevent
  // flooding chromium's dispatch loop, which can cause ipc traffic
  // processing lag.
  QUIRK_FLASH_THROTTLE_WMUSER_EVENTS              = 1 << 2,
  // Win32: Catch resets on our subclass by hooking SetWindowLong.
  QUIRK_FLASH_HOOK_SETLONGPTR                     = 1 << 3,
  // X11: Work around a bug in Flash up to 10.1 d51 at least, where
  // expose event top left coordinates within the plugin-rect and
  // not at the drawable origin are misinterpreted.
  QUIRK_FLASH_EXPOSE_COORD_TRANSLATION            = 1 << 4,
  // Win32: Catch get window info calls on the browser and tweak the
  // results so mouse input works when flash is displaying it's settings
  // window.
  QUIRK_FLASH_HOOK_GETWINDOWINFO                  = 1 << 5,
  // Win: Addresses a flash bug with mouse capture and full screen
  // windows.
  QUIRK_FLASH_FIXUP_MOUSE_CAPTURE                 = 1 << 6,
  // Win: Check to make sure the parent window has focus before calling
  // set focus on the child. Addresses a full screen dialog prompt
  // problem in Silverlight.
  QUIRK_SILVERLIGHT_FOCUS_CHECK_PARENT            = 1 << 8,
  // Mac: Allow the plugin to use offline renderer mode.
  // Use this only if the plugin is certified the support the offline renderer.
  QUIRK_ALLOW_OFFLINE_RENDERER                    = 1 << 9,
  // Work around a Flash bug where it fails to check the error code of a
  // NPN_GetValue(NPNVdocumentOrigin) call before trying to dereference
  // its char* output.
  QUIRK_FLASH_RETURN_EMPTY_DOCUMENT_ORIGIN        = 1 << 10,
  // Win: Addresses a Unity bug with mouse capture.
  QUIRK_UNITY_FIXUP_MOUSE_CAPTURE                 = 1 << 11,
  // Win: Hook IMM32 API to handle IME event on windowless plugin
  QUIRK_WINLESS_HOOK_IME                          = 1 << 12,
  // Win: Hook GetKeyState to get keyboard state on sandbox process
  QUIRK_FLASH_HOOK_GETKEYSTATE                    = 1 << 13,
};

int GetQuirksFromMimeTypeAndFilename(const nsCString& aMimeType,
                                     const nsCString& aPluginFilename);

} /* namespace plugins */
} /* namespace mozilla */

#endif  // ifndef dom_plugins_PluginQuirks_h
