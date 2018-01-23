/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { ShellService } = Cu.import("resource:///modules/ShellService.jsm", {});

add_task(async function test_default_browser_check() {
  ShellService._checkedThisSession = false;
  // On a normal profile, the default is true. However, this gets set to false on the
  // testing profile. Let's start with true for a sanity check.

  ShellService.shouldCheckDefaultBrowser = true;
  is(ShellService.shouldCheckDefaultBrowser, true, "Sanity check");

  await setupPolicyEngineWithJson("config_dont_check_default_browser.json");

  is(ShellService.shouldCheckDefaultBrowser, false, "Policy changed it to not check");

  // Try to change it to true and check that it doesn't take effect
  ShellService.shouldCheckDefaultBrowser = true;

  is(ShellService.shouldCheckDefaultBrowser, false, "Policy is enforced");

  // Unlock the pref because if it stays locked, and this test runs twice in a row,
  // the first sanity check will fail.
  Services.prefs.unlockPref("browser.shell.checkDefaultBrowser");
});
