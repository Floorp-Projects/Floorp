/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test passing an empty list of pref names to the module.
 */

add_task(function() {
  const PREFS_LIST = [];

  startModule(PREFS_LIST);

  Assert.throws(
    getRegistryKey,
    /NS_ERROR_FAILURE/,
    "The registry key shouldn't have been created, so opening it should fail"
  );
});
