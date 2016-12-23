/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Test the storage inspector when dom.caches.enabled=false.

add_task(function* () {
  // Disable the DOM cache
  Services.prefs.setBoolPref(DOM_CACHE, false);

  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  const state = [
    [["localStorage", "http://test1.example.org"],
      ["ls1", "ls2"]],
    [["localStorage", "http://sectest1.example.org"],
      ["iframe-u-ls1"]],
    [["localStorage", "https://sectest1.example.org"],
      ["iframe-s-ls1"]],
    [["sessionStorage", "http://test1.example.org"],
      ["ss1"]],
    [["sessionStorage", "http://sectest1.example.org"],
      ["iframe-u-ss1", "iframe-u-ss2"]],
    [["sessionStorage", "https://sectest1.example.org"],
      ["iframe-s-ss1"]],
    [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"],
      [1, 2, 3]],
  ];

  yield checkState(state);

  yield finishTests();
});
