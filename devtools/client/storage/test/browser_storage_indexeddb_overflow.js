/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test endless scrolling when a lot of items are present in the storage
// inspector table for IndexedDB.
"use strict";

const ITEMS_PER_PAGE = 50;

add_task(async function () {
  await openTabAndSetupStorage(
    MAIN_DOMAIN_SECURED + "storage-overflow-indexeddb.html"
  );

  info("Run the tests with short DevTools");
  await runTests();

  info("Close Toolbox");
  await gDevTools.closeToolboxForTab(gBrowser.selectedTab);
});

async function runTests() {
  gUI.tree.expandAll();

  await selectTreeItem([
    "indexedDB",
    "https://test1.example.org",
    "database (default)",
    "store",
  ]);
  checkCellLength(ITEMS_PER_PAGE);

  await scroll();
  checkCellLength(ITEMS_PER_PAGE * 2);
}
