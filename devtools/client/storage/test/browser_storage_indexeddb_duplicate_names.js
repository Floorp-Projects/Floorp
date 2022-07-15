/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that indexedDBs with duplicate names (different types / paths)
// work as expected.

"use strict";

add_task(async function() {
  const TESTPAGE =
    MAIN_DOMAIN_SECURED + "storage-indexeddb-duplicate-names.html";

  setPermission(TESTPAGE, "indexedDB");

  await openTabAndSetupStorage(TESTPAGE);

  await checkState([
    [
      ["indexedDB", "https://test1.example.org"],
      ["idb1 (default)", "idb2 (default)"],
    ],
  ]);
});
