/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to assert that the storage tree and table corresponding to each
// item in the storage tree is correctly displayed

// Entries that should be present in the tree for this test
// Format for each entry in the array :
// [
//   ["path", "to", "tree", "item"], - The path to the tree item to click formed
//                                     by id of each item
//   ["key_value1", "key_value2", ...] - The value of the first (unique) column
//                                       for each row in the table corresponding
//                                       to the tree item selected.
// ]
// These entries are formed by the cookies, local storage, session storage and
// indexedDB entries created in storage-listings.html,
// storage-secured-iframe.html and storage-unsecured-iframe.html

"use strict";

const testCases = [
  [["cookies", "test1.example.org"],
   ["c1", "cs2", "c3", "uc1"]],
  [["cookies", "sectest1.example.org"],
   ["uc1", "cs2", "sc1"]],
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
  [["indexedDB", "http://test1.example.org"],
   ["idb1", "idb2"]],
  [["indexedDB", "http://test1.example.org", "idb1"],
   ["obj1", "obj2"]],
  [["indexedDB", "http://test1.example.org", "idb2"],
   ["obj3"]],
  [["indexedDB", "http://test1.example.org", "idb1", "obj1"],
   [1, 2, 3]],
  [["indexedDB", "http://test1.example.org", "idb1", "obj2"],
   [1]],
  [["indexedDB", "http://test1.example.org", "idb2", "obj3"],
   []],
  [["indexedDB", "http://sectest1.example.org"],
   []],
  [["indexedDB", "https://sectest1.example.org"],
   ["idb-s1", "idb-s2"]],
  [["indexedDB", "https://sectest1.example.org", "idb-s1"],
   ["obj-s1"]],
  [["indexedDB", "https://sectest1.example.org", "idb-s2"],
   ["obj-s2"]],
  [["indexedDB", "https://sectest1.example.org", "idb-s1", "obj-s1"],
   [6, 7]],
  [["indexedDB", "https://sectest1.example.org", "idb-s2", "obj-s2"],
   [16]],
];

/**
 * Test that the desired number of tree items are present
 */
function testTree() {
  let doc = gPanelWindow.document;
  for (let item of testCases) {
    ok(doc.querySelector("[data-id='" + JSON.stringify(item[0]) + "']"),
       "Tree item " + item[0] + " should be present in the storage tree");
  }
}

/**
 * Test that correct table entries are shown for each of the tree item
 */
function* testTables() {
  let doc = gPanelWindow.document;
  // Expand all nodes so that the synthesized click event actually works
  gUI.tree.expandAll();

  // First tree item is already selected so no clicking and waiting for update
  for (let id of testCases[0][1]) {
    ok(doc.querySelector(".table-widget-cell[data-id='" + id + "']"),
       "Table item " + id + " should be present");
  }

  // Click rest of the tree items and wait for the table to be updated
  for (let item of testCases.slice(1)) {
    yield selectTreeItem(item[0]);

    // Check whether correct number of items are present in the table
    is(doc.querySelectorAll(
         ".table-widget-wrapper:first-of-type .table-widget-cell"
       ).length, item[1].length, "Number of items in table is correct");

    // Check if all the desired items are present in the table
    for (let id of item[1]) {
      ok(doc.querySelector(".table-widget-cell[data-id='" + id + "']"),
         "Table item " + id + " should be present");
    }
  }
}

add_task(function*() {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  testTree();
  yield testTables();

  yield finishTests();
});
