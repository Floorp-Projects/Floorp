/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that pref values are correctly updated when they change.
 */

add_task(function() {
  const PREFS_LIST = ["installer.pref"];

  Services.prefs.setBoolPref("installer.pref", true);

  startModule(PREFS_LIST);

  verifyReflectedPrefs(PREFS_LIST);

  Services.prefs.setBoolPref("installer.pref", false);

  verifyReflectedPrefs(PREFS_LIST);

  Services.prefs.setBoolPref("installer.pref", true);

  verifyReflectedPrefs(PREFS_LIST);
});
