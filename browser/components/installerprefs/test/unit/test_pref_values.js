/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that pref values are reflected correctly.
 */

add_task(function() {
  const PREFS_LIST = ["installer.true.value", "installer.false.value"];

  Services.prefs.setBoolPref("installer.true.value", true);
  Services.prefs.setBoolPref("installer.false.value", false);

  startModule(PREFS_LIST);

  verifyReflectedPrefs(PREFS_LIST);
});
