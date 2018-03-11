/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that indexedDBs with duplicate names (different types / paths)
// work as expected.

"use strict";

add_task(async function() {
  const TESTPAGE = MAIN_DOMAIN + "storage-indexeddb-duplicate-names.html";

  setPermission(TESTPAGE, "indexedDB");

  await openTabAndSetupStorage(TESTPAGE);

  await checkState([
    [
      ["indexedDB", "http://test1.example.org"], [
        "idb1 (default)",
        "idb1 (temporary)",
        "idb1 (persistent)",
        "idb2 (default)",
        "idb2 (temporary)",
        "idb2 (persistent)"
      ]
    ]
  ]);

  await finishTests();
});
