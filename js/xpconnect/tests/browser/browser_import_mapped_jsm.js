/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// Verify Cu.import and ChromeUtils.import works for JSM URL even after
// ESM-ification, and any not-in-tree consumer doesn't break.
//
// This test modules that's commonly used by not-in-tree consumers, such as
// privilege extensions and AutoConfigs.

const JSMs = [
  "resource:///modules/AboutNewTab.jsm",
  "resource:///modules/CustomizableUI.jsm",
  "resource:///modules/UITour.jsm",
  "resource:///modules/distribution.js",
  "resource://gre/modules/AddonManager.jsm",
  "resource://gre/modules/AppConstants.jsm",
  "resource://gre/modules/AsyncShutdown.jsm",
  "resource://gre/modules/Console.jsm",
  "resource://gre/modules/FileUtils.jsm",
  "resource://gre/modules/LightweightThemeManager.jsm",
  "resource://gre/modules/NetUtil.jsm",
  "resource://gre/modules/PlacesUtils.jsm",
  "resource://gre/modules/PluralForm.jsm",
  "resource://gre/modules/PrivateBrowsingUtils.jsm",
  "resource://gre/modules/Timer.jsm",
  "resource://gre/modules/XPCOMUtils.jsm",
  "resource://gre/modules/osfile.jsm",
  "resource://gre/modules/addons/XPIDatabase.jsm",
  "resource://gre/modules/addons/XPIProvider.jsm",
  "resource://gre/modules/addons/XPIInstall.jsm",
  "resource:///modules/BrowserWindowTracker.jsm",
];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

if (AppConstants.platform === "win") {
  JSMs.push("resource:///modules/WindowsJumpLists.jsm");
}

add_task(async function test_chrome_utils_import() {
  for (const file of JSMs) {
    try {
      ChromeUtils.import(file);
      ok(true, `Imported ${file}`);
    } catch (e) {
      ok(false, `Failed to import ${file}`);
    }
  }
});

add_task(async function test_cu_import() {
  for (const file of JSMs) {
    try {
      Cu.import(file, {});
      ok(true, `Imported ${file}`);
    } catch (e) {
      ok(false, `Failed to import ${file}`);
    }
  }
});
