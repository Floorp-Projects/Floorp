/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that prefs of types other than bool are not reflected.
 */

add_task(function() {
  const PREFS_LIST = ["installer.int", "installer.string"];

  Services.prefs.setIntPref("installer.int", 12);
  Services.prefs.setStringPref("installer.string", "I'm a string");

  startModule(PREFS_LIST);

  verifyReflectedPrefs(PREFS_LIST);
});
