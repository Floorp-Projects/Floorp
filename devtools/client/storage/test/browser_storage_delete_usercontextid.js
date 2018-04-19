/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Test deleting storage items with userContextId.

// The items that will be deleted.
const TEST_CASES = [
  [["localStorage", "http://test1.example.org"],
   "ls1", "name"],
  [["sessionStorage", "http://test1.example.org"],
   "ss1", "name"],
  [
    ["cookies", "http://test1.example.org"],
    getCookieId("c1", "test1.example.org", "/browser"), "name"
  ],
  [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"],
   1, "name"],
  [["Cache", "http://test1.example.org", "plop"],
   MAIN_DOMAIN + "404_cached_file.js", "url"],
];

// The storage items that should exist for default userContextId
const storageItemsForDefault = [
  [
    ["cookies", "http://test1.example.org"],
    [
      getCookieId("c1", "test1.example.org", "/browser"),
      getCookieId("cs2", ".example.org", "/"),
      getCookieId("c3", "test1.example.org", "/"),
      getCookieId("c4", ".example.org", "/"),
      getCookieId("uc1", ".example.org", "/"),
      getCookieId("uc2", ".example.org", "/")
    ]
  ],
  [
    ["cookies", "https://sectest1.example.org"],
    [
      getCookieId("uc1", ".example.org", "/"),
      getCookieId("uc2", ".example.org", "/"),
      getCookieId("cs2", ".example.org", "/"),
      getCookieId("c4", ".example.org", "/"),
      getCookieId("sc1", "sectest1.example.org",
        "/browser/devtools/client/storage/test/"),
      getCookieId("sc2", "sectest1.example.org",
        "/browser/devtools/client/storage/test/")
    ]
  ],
  [["localStorage", "http://test1.example.org"],
   ["key", "ls1", "ls2"]],
  [["localStorage", "http://sectest1.example.org"],
   ["iframe-u-ls1"]],
  [["localStorage", "https://sectest1.example.org"],
   ["iframe-s-ls1"]],
  [["sessionStorage", "http://test1.example.org"],
   ["key", "ss1"]],
  [["sessionStorage", "http://sectest1.example.org"],
   ["iframe-u-ss1", "iframe-u-ss2"]],
  [["sessionStorage", "https://sectest1.example.org"],
   ["iframe-s-ss1"]],
  [["indexedDB", "http://test1.example.org"],
   ["idb1 (default)", "idb2 (default)"]],
  [["indexedDB", "http://test1.example.org", "idb1 (default)"],
   ["obj1", "obj2"]],
  [["indexedDB", "http://test1.example.org", "idb2 (default)"],
   ["obj3"]],
  [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"],
   [1, 2, 3]],
  [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj2"],
   [1]],
  [["indexedDB", "http://test1.example.org", "idb2 (default)", "obj3"],
   []],
  [["indexedDB", "http://sectest1.example.org"],
   []],
  [["indexedDB", "https://sectest1.example.org"],
   ["idb-s1 (default)", "idb-s2 (default)"]],
  [["indexedDB", "https://sectest1.example.org", "idb-s1 (default)"],
   ["obj-s1"]],
  [["indexedDB", "https://sectest1.example.org", "idb-s2 (default)"],
   ["obj-s2"]],
  [["indexedDB", "https://sectest1.example.org", "idb-s1 (default)", "obj-s1"],
   [6, 7]],
  [["indexedDB", "https://sectest1.example.org", "idb-s2 (default)", "obj-s2"],
   [16]],
  [["Cache", "http://test1.example.org", "plop"],
   [MAIN_DOMAIN + "404_cached_file.js",
    MAIN_DOMAIN + "browser_storage_basic.js"]],
];

/**
 * Test that the desired number of tree items are present
 */
function testTree(tests) {
  let doc = gPanelWindow.document;
  for (let [item] of tests) {
    ok(doc.querySelector("[data-id='" + JSON.stringify(item) + "']"),
      `Tree item ${item.toSource()} should be present in the storage tree`);
  }
}

/**
 * Test that correct table entries are shown for each of the tree item
 */
async function testTables(tests) {
  let doc = gPanelWindow.document;
  // Expand all nodes so that the synthesized click event actually works
  gUI.tree.expandAll();

  // First tree item is already selected so no clicking and waiting for update
  for (let id of tests[0][1]) {
    ok(doc.querySelector(".table-widget-cell[data-id='" + id + "']"),
       "Table item " + id + " should be present");
  }

  // Click rest of the tree items and wait for the table to be updated
  for (let [treeItem, items] of tests.slice(1)) {
    await selectTreeItem(treeItem);

    // Check whether correct number of items are present in the table
    is(doc.querySelectorAll(
         ".table-widget-wrapper:first-of-type .table-widget-cell"
       ).length, items.length, "Number of items in table is correct");

    // Check if all the desired items are present in the table
    for (let id of items) {
      ok(doc.querySelector(".table-widget-cell[data-id='" + id + "']"),
         "Table item " + id + " should be present");
    }
  }
}

add_task(async function() {
  // First, open a tab with the default userContextId and setup its storages.
  let tabDefault = await openTab(MAIN_DOMAIN + "storage-listings.html");

  // Second, start testing for userContextId 1.
  // We use the same item name as the default page has to see deleting items
  // from userContextId 1 will affect default one or not.
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html", {userContextId: 1});

  let contextMenu = gPanelWindow.document.getElementById("storage-table-popup");
  let menuDeleteItem = contextMenu.querySelector("#storage-table-popup-delete");

  for (let [ treeItem, rowName, cellToClick] of TEST_CASES) {
    let treeItemName = treeItem.join(" > ");

    info(`Selecting tree item ${treeItemName}`);
    await selectTreeItem(treeItem);

    let row = getRowCells(rowName);
    ok(gUI.table.items.has(rowName), `There is a row '${rowName}' in ${treeItemName}`);

    let eventWait = gUI.once("store-objects-edit");

    await waitForContextMenu(contextMenu, row[cellToClick], () => {
      info(`Opened context menu in ${treeItemName}, row '${rowName}'`);
      menuDeleteItem.click();
      let truncatedRowName = String(rowName).replace(SEPARATOR_GUID, "-").substr(0, 16);
      ok(menuDeleteItem.getAttribute("label").includes(truncatedRowName),
        `Context menu item label contains '${rowName}' (maybe truncated)`);
    });

    await eventWait;

    ok(!gUI.table.items.has(rowName),
      `There is no row '${rowName}' in ${treeItemName} after deletion`);
  }

  // Final, we see that the default tab is intact or not.
  await BrowserTestUtils.switchTab(gBrowser, tabDefault);
  await openStoragePanel();

  testTree(storageItemsForDefault);
  await testTables(storageItemsForDefault);

  await finishTests();
});
