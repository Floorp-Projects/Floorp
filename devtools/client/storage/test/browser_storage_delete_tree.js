/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Test deleting all storage items from the tree.

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  let contextMenu = gPanelWindow.document.getElementById("storage-tree-popup");
  let menuDeleteAllItem = contextMenu.querySelector(
    "#storage-tree-popup-delete-all");

  info("test state before delete");
  yield checkState([
    [["cookies", "test1.example.org"], ["c1", "c3", "cs2", "uc1"]],
    [["localStorage", "http://test1.example.org"], ["ls1", "ls2"]],
    [["sessionStorage", "http://test1.example.org"], ["ss1"]],
    [["indexedDB", "http://test1.example.org", "idb1", "obj1"], [1, 2, 3]],
  ]);

  info("do the delete");
  const deleteHosts = [
    ["cookies", "test1.example.org"],
    ["localStorage", "http://test1.example.org"],
    ["sessionStorage", "http://test1.example.org"],
    ["indexedDB", "http://test1.example.org", "idb1", "obj1"],
  ];

  for (let store of deleteHosts) {
    let storeName = store.join(" > ");

    yield selectTreeItem(store);

    let eventName = "store-objects-" +
      (store[0] == "cookies" ? "updated" : "cleared");
    let eventWait = gUI.once(eventName);

    let selector = `[data-id='${JSON.stringify(store)}'] > .tree-widget-item`;
    let target = gPanelWindow.document.querySelector(selector);
    ok(target, `tree item found in ${storeName}`);
    yield waitForContextMenu(contextMenu, target, () => {
      info(`Opened tree context menu in ${storeName}`);
      menuDeleteAllItem.click();
    });

    yield eventWait;
  }

  info("test state after delete");
  yield checkState([
    [["cookies", "test1.example.org"], []],
    [["localStorage", "http://test1.example.org"], []],
    [["sessionStorage", "http://test1.example.org"], []],
    [["indexedDB", "http://test1.example.org", "idb1", "obj1"], []],
  ]);

  yield finishTests();
});
