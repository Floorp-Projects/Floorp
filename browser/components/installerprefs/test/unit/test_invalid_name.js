/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that using prefs with invalid names is not allowed.
 */

add_task(function() {
  const PREFS_LIST = [
    "the.wrong.branch",
    "installer.the.right.branch",
    "not.a.real.pref",
  ];

  startModule(PREFS_LIST);

  verifyReflectedPrefs(PREFS_LIST);
});
