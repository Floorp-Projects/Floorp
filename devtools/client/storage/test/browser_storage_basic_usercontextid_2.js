/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A test to check that the storage inspector is working correctly with
// userContextId.

"use strict";

const testCasesUserContextId = [
  [
    ["cookies", "http://test1.example.org"],
    [
      getCookieId("c1uc1", "test1.example.org", "/browser"),
      getCookieId("cs2uc1", ".example.org", "/"),
      getCookieId("c3uc1", "test1.example.org", "/"),
      getCookieId("uc1uc1", ".example.org", "/")
    ]
  ],
  [
    ["cookies", "https://sectest1.example.org"],
    [
      getCookieId("uc1uc1", ".example.org", "/"),
      getCookieId("cs2uc1", ".example.org", "/"),
      getCookieId("sc1uc1", "sectest1.example.org",
        "/browser/devtools/client/storage/test/")
    ]
  ],
  [["localStorage", "http://test1.example.org"],
   ["ls1uc1", "ls2uc1"]],
  [["localStorage", "http://sectest1.example.org"],
   ["iframe-u-ls1uc1"]],
  [["localStorage", "https://sectest1.example.org"],
   ["iframe-s-ls1uc1"]],
  [["sessionStorage", "http://test1.example.org"],
   ["ss1uc1"]],
  [["sessionStorage", "http://sectest1.example.org"],
   ["iframe-u-ss1uc1", "iframe-u-ss2uc1"]],
  [["sessionStorage", "https://sectest1.example.org"],
   ["iframe-s-ss1uc1"]],
  [["indexedDB", "http://test1.example.org"],
   ["idb1uc1 (default)", "idb2uc1 (default)"]],
  [["indexedDB", "http://test1.example.org", "idb1uc1 (default)"],
   ["obj1uc1", "obj2uc1"]],
  [["indexedDB", "http://test1.example.org", "idb2uc1 (default)"],
   ["obj3uc1"]],
  [["indexedDB", "http://test1.example.org", "idb1uc1 (default)", "obj1uc1"],
   [1, 2, 3]],
  [["indexedDB", "http://test1.example.org", "idb1uc1 (default)", "obj2uc1"],
   [1]],
  [["indexedDB", "http://test1.example.org", "idb2uc1 (default)", "obj3uc1"],
   []],
  [["indexedDB", "http://sectest1.example.org"],
   []],
  [["indexedDB", "https://sectest1.example.org"],
   ["idb-s1uc1 (default)", "idb-s2uc1 (default)"]],
  [["indexedDB", "https://sectest1.example.org", "idb-s1uc1 (default)"],
   ["obj-s1uc1"]],
  [["indexedDB", "https://sectest1.example.org", "idb-s2uc1 (default)"],
   ["obj-s2uc1"]],
  [["indexedDB", "https://sectest1.example.org", "idb-s1uc1 (default)", "obj-s1uc1"],
   [6, 7]],
  [["indexedDB", "https://sectest1.example.org", "idb-s2uc1 (default)", "obj-s2uc1"],
   [16]],
  [["Cache", "http://test1.example.org", "plopuc1"],
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
       "Tree item " + item[0] + " should be present in the storage tree");
  }
}

/**
 * Test that correct table entries are shown for each of the tree item
 */
function* testTables(tests) {
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
    yield selectTreeItem(treeItem);

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

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings-usercontextid.html",
                               {userContextId: 1});

  testTree(testCasesUserContextId);
  yield testTables(testCasesUserContextId);

  yield finishTests();
});
