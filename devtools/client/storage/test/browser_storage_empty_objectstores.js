/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to assert that the storage tree and table corresponding to each
// item in the storage tree is correctly displayed.

"use strict";

// Entries that should be present in the tree for this test
// Format for each entry in the array:
// [
//   ["path", "to", "tree", "item"],
//   - The path to the tree item to click formed by id of each item
//   ["key_value1", "key_value2", ...]
//   - The value of the first (unique) column for each row in the table
//     corresponding to the tree item selected.
// ]
// These entries are formed by the cookies, local storage, session storage and
// indexedDB entries created in storage-listings.html,
// storage-secured-iframe.html and storage-unsecured-iframe.html
const storeItems = [
  [["indexedDB", "http://test1.example.org"],
   ["idb1 (default)", "idb2 (default)"]],
  [["indexedDB", "http://test1.example.org", "idb1 (default)"],
   ["obj1", "obj2"]],
  [["indexedDB", "http://test1.example.org", "idb2 (default)"],
   []],
  [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"],
   [1, 2, 3]],
  [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj2"],
   [1]]
];

/**
 * Test that the desired number of tree items are present
 */
function testTree() {
  let doc = gPanelWindow.document;
  for (let [item] of storeItems) {
    ok(doc.querySelector(`[data-id='${JSON.stringify(item)}']`),
      `Tree item ${item} should be present in the storage tree`);
  }
}

/**
 * Test that correct table entries are shown for each of the tree item
 */
let testTables = function* () {
  let doc = gPanelWindow.document;
  // Expand all nodes so that the synthesized click event actually works
  gUI.tree.expandAll();

  // Click the tree items and wait for the table to be updated
  for (let [item, ids] of storeItems) {
    yield selectTreeItem(item);

    // Check whether correct number of items are present in the table
    is(doc.querySelectorAll(
         ".table-widget-wrapper:first-of-type .table-widget-cell"
       ).length, ids.length, "Number of items in table is correct");

    // Check if all the desired items are present in the table
    for (let id of ids) {
      ok(doc.querySelector(".table-widget-cell[data-id='" + id + "']"),
        `Table item ${id} should be present`);
    }
  }
};

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-empty-objectstores.html");

  testTree();
  yield testTables();
  yield finishTests();
});
