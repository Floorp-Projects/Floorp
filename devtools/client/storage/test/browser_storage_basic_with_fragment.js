/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

// A second basic test to assert that the storage tree and table corresponding
// to each item in the storage tree is correctly displayed.

// This test differs from browser_storage_basic.js because the URLs we load
// include fragments e.g. http://example.com/test.js#abcdefg
//                                                  ^^^^^^^^
//                                                  fragment

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
  [
    ["cookies", "http://test1.example.org"],
    [
      getCookieId("c1", "test1.example.org", "/browser"),
      getCookieId("cs2", ".example.org", "/"),
      getCookieId("c3", "test1.example.org", "/"),
      getCookieId("uc1", ".example.org", "/"),
      getCookieId("uc2", ".example.org", "/"),
    ],
  ],
  [
    ["cookies", "https://sectest1.example.org"],
    [
      getCookieId("uc1", ".example.org", "/"),
      getCookieId("uc2", ".example.org", "/"),
      getCookieId("cs2", ".example.org", "/"),
      getCookieId(
        "sc1",
        "sectest1.example.org",
        "/browser/devtools/client/storage/test"
      ),
      getCookieId(
        "sc2",
        "sectest1.example.org",
        "/browser/devtools/client/storage/test"
      ),
    ],
  ],
  [
    ["localStorage", "http://test1.example.org"],
    ["ls1", "ls2"],
  ],
  [["localStorage", "http://sectest1.example.org"], ["iframe-u-ls1"]],
  [["localStorage", "https://sectest1.example.org"], ["iframe-s-ls1"]],
  [["sessionStorage", "http://test1.example.org"], ["ss1"]],
  [
    ["sessionStorage", "http://sectest1.example.org"],
    ["iframe-u-ss1", "iframe-u-ss2"],
  ],
  [["sessionStorage", "https://sectest1.example.org"], ["iframe-s-ss1"]],
  [
    ["indexedDB", "http://test1.example.org"],
    ["idb1 (default)", "idb2 (default)"],
  ],
  [
    ["indexedDB", "http://test1.example.org", "idb1 (default)"],
    ["obj1", "obj2"],
  ],
  [["indexedDB", "http://test1.example.org", "idb2 (default)"], ["obj3"]],
  [
    ["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"],
    [1, 2, 3],
  ],
  [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj2"], [1]],
  [["indexedDB", "http://test1.example.org", "idb2 (default)", "obj3"], []],
  [["indexedDB", "http://sectest1.example.org"], []],
  [
    ["indexedDB", "https://sectest1.example.org"],
    ["idb-s1 (default)", "idb-s2 (default)"],
  ],
  [
    ["indexedDB", "https://sectest1.example.org", "idb-s1 (default)"],
    ["obj-s1"],
  ],
  [
    ["indexedDB", "https://sectest1.example.org", "idb-s2 (default)"],
    ["obj-s2"],
  ],
  [
    ["indexedDB", "https://sectest1.example.org", "idb-s1 (default)", "obj-s1"],
    [6, 7],
  ],
  [
    ["indexedDB", "https://sectest1.example.org", "idb-s2 (default)", "obj-s2"],
    [16],
  ],
  [
    ["Cache", "http://test1.example.org", "plop"],
    [
      MAIN_DOMAIN + "404_cached_file.js",
      MAIN_DOMAIN + "browser_storage_basic.js",
    ],
  ],
];

/**
 * Test that the desired number of tree items are present
 */
function testTree() {
  const doc = gPanelWindow.document;
  for (const [item] of testCases) {
    ok(
      doc.querySelector("[data-id='" + JSON.stringify(item) + "']"),
      `Tree item ${item.toSource()} should be present in the storage tree`
    );
  }
}

/**
 * Test that correct table entries are shown for each of the tree item
 */
async function testTables() {
  const doc = gPanelWindow.document;
  // Expand all nodes so that the synthesized click event actually works
  gUI.tree.expandAll();

  // First tree item is already selected so no clicking and waiting for update
  for (const id of testCases[0][1]) {
    ok(
      doc.querySelector(".table-widget-cell[data-id='" + id + "']"),
      "Table item " + id + " should be present"
    );
  }

  // Click rest of the tree items and wait for the table to be updated
  for (const [treeItem, items] of testCases.slice(1)) {
    await selectTreeItem(treeItem);

    // Check whether correct number of items are present in the table
    is(
      doc.querySelectorAll(
        ".table-widget-wrapper:first-of-type .table-widget-cell"
      ).length,
      items.length,
      "Number of items in table is correct"
    );

    // Check if all the desired items are present in the table
    for (const id of items) {
      ok(
        doc.querySelector(".table-widget-cell[data-id='" + id + "']"),
        "Table item " + id + " should be present"
      );
    }
  }
}

add_task(async function() {
  // storage-listings.html explicitly mixes secure and insecure frames.
  // We should not enforce https for tests using this page.
  await pushPref("dom.security.https_first", false);

  await openTabAndSetupStorage(
    MAIN_DOMAIN + "storage-listings-with-fragment.html#abc"
  );

  testTree();
  await testTables();
});
