/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Test deleting all storage items from the tree.

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  const contextMenu = gPanelWindow.document.getElementById("storage-tree-popup");
  const menuDeleteAllItem = contextMenu.querySelector(
    "#storage-tree-popup-delete-all");

  info("test state before delete");
  await checkState([
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
    [["localStorage", "http://test1.example.org"], ["key", "ls1", "ls2"]],
    [["sessionStorage", "http://test1.example.org"], ["key", "ss1"]],
    [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"], [1, 2, 3]],
    [["Cache", "http://test1.example.org", "plop"],
      [MAIN_DOMAIN + "404_cached_file.js", MAIN_DOMAIN + "browser_storage_basic.js"]],
  ]);

  info("do the delete");
  const deleteHosts = [
    ["cookies", "http://test1.example.org"],
    ["localStorage", "http://test1.example.org"],
    ["sessionStorage", "http://test1.example.org"],
    ["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"],
    ["Cache", "http://test1.example.org", "plop"],
  ];

  for (const store of deleteHosts) {
    const storeName = store.join(" > ");

    await selectTreeItem(store);

    const eventName = "store-objects-" +
      (store[0] == "cookies" ? "edit" : "cleared");
    const eventWait = gUI.once(eventName);

    const selector = `[data-id='${JSON.stringify(store)}'] > .tree-widget-item`;
    const target = gPanelWindow.document.querySelector(selector);
    ok(target, `tree item found in ${storeName}`);
    await waitForContextMenu(contextMenu, target, () => {
      info(`Opened tree context menu in ${storeName}`);
      menuDeleteAllItem.click();
    });

    await eventWait;
  }

  info("test state after delete");
  await checkState([
    [["cookies", "http://test1.example.org"], []],
    [["localStorage", "http://test1.example.org"], []],
    [["sessionStorage", "http://test1.example.org"], []],
    [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"], []],
    [["Cache", "http://test1.example.org", "plop"], []],
  ]);

  await finishTests();
});
