// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This header specifies extensions to the Chrome Plugin API to support Gears.

#ifndef CHROME_COMMON_GEARS_API_H__
#define CHROME_COMMON_GEARS_API_H__

#include "chrome/common/chrome_plugin_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// CommandIDs used when Chrome calls into Gears using CPP_HandleCommand.
// Note: do not change the enum values.  We want to preserve backwards
// compatibility.
typedef enum {
  // Ask gears to show its settings dialog.  Typical usage is for the plugin
  // to display it using a call to CPB_ShowHtmlDialog.  No command_data is
  // provided.
  GEARSPLUGINCOMMAND_SHOW_SETTINGS = 0,

  // Ask gears to create a shortcut to a web page.  command_data points
  // to a GearsShortcutData struct.
  GEARSPLUGINCOMMAND_CREATE_SHORTCUT = 1,

  // Query gears for the list of installed shortcuts.  command_data points
  // to a GearsShortcutList struct.
  GEARSPLUGINCOMMAND_GET_SHORTCUT_LIST = 2,
} GearsPluginCommand;

// CommandIDs used when Gears calls into Chrome using CPB_HandleCommand.
// Note: do not change the enum values.  We want to preserve backwards
// compatibility.
typedef enum {
  // Tell chrome that the GEARSPLUGINCOMMAND_CREATE_SHORTCUT command is done,
  // and the user has closed the dialog.  command_data points to the same
  // GearsShortcutData struct that was passed to the plugin command.
  GEARSBROWSERCOMMAND_CREATE_SHORTCUT_DONE = 1,

  // Notifies the browser of changes to the gears shortcuts database.
  // command_data is null.
  GEARSBROWSERCOMMAND_NOTIFY_SHORTCUTS_CHANGED = 3,
} GearsBrowserCommand;

// Note: currently only 16x16, 32x32, 48x48, and 128x128 icons are supported.
typedef struct _GearsShortcutIcon {
  const char* size;  // unused
  const char* url;  // the URL of the icon, which should be a PNG image
  int width;  // width of the icon
  int height;  // height of the icon
} GearsShortcutIcon;

// Command data for GEARSPLUGINCOMMAND_CREATE_SHORTCUT.
typedef struct _GearsShortcutData {
  const char* name;  // the shortcut's name (also used as the filename)
  const char* url;  // the URL that the shortcut should launch
  const char* description;  // an optional description
  GearsShortcutIcon icons[4];  // list of icons to use for this shortcut
} GearsShortcutData;

// Command data for GEARSPLUGINCOMMAND_CREATE_SHORTCUT used in 0.6 and later.
// This struct is backwards compatible with the first version.
// http://b/viewIssue?id=1331408 - Chrome sanitizes 'name' for compatibility
// with older versions of Gears that expect this.  'orig_name' is unsanitized,
// which allows Gears to do its own validation.
typedef struct _GearsShortcutData2 {
  const char* name;  // unused - for back compat with above struct
  const char* url;  // the URL that the shortcut should launch
  const char* description;  // an optional description
  GearsShortcutIcon icons[4];  // list of icons to use for this shortcut
  const char* orig_name;  // the shortcut's unmodified filename (added in 0.6)
} GearsShortcutData2;

// Command data for GEARSPLUGINCOMMAND_GET_SHORTCUT_LIST.
typedef struct _GearsShortcutList {
  // Note: these are output params, set by Gears.  There are no input params.
  // Memory for these shortcuts, including the strings they hold, should be
  // freed by the browser using CPB_Free.
  GearsShortcutData* shortcuts;  // array of installed shortcuts
  uint32_t num_shortcuts;  // size of the array
} GearsShortcutList;

// Command data for GEARSBROWSERCOMMAND_CREATE_SHORTCUT_DONE
typedef struct _GearsCreateShortcutResult {
  GearsShortcutData2* shortcut;  // pointer to struct passed to
                                 // GEARSPLUGINCOMMAND_CREATE_SHORTCUT
  CPError result;  // CPERR_SUCCESS if shortcut was created, or error otherwise
} GearsCreateShortcutResult;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CHROME_COMMON_GEARS_API_H__
