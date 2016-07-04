/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Test deleting all storage items

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  let contextMenu = gPanelWindow.document.getElementById("storage-table-popup");
  let menuDeleteAllItem = contextMenu.querySelector(
    "#storage-table-popup-delete-all");

  info("test state before delete");
  const beforeState = [
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
    [["indexedDB", "http://test1.example.org", "idb1", "obj1"],
      [1, 2, 3]],
  ];

  yield checkState(beforeState);

  info("do the delete");
  const deleteHosts = [
    [["localStorage", "https://sectest1.example.org"], "iframe-s-ls1"],
    [["sessionStorage", "https://sectest1.example.org"], "iframe-s-ss1"],
    [["indexedDB", "http://test1.example.org", "idb1", "obj1"], 1],
  ];

  for (let [store, rowName] of deleteHosts) {
    let storeName = store.join(" > ");

    yield selectTreeItem(store);

    let eventWait = gUI.once("store-objects-cleared");

    let cell = getRowCells(rowName).name;
    yield waitForContextMenu(contextMenu, cell, () => {
      info(`Opened context menu in ${storeName}, row '${rowName}'`);
      menuDeleteAllItem.click();
    });

    yield eventWait;
  }

  info("test state after delete");
  const afterState = [
    // iframes from the same host, one secure, one unsecure, are independent
    // from each other. Delete all in one doesn't touch the other one.
    [["localStorage", "http://test1.example.org"],
      ["ls1", "ls2"]],
    [["localStorage", "http://sectest1.example.org"],
      ["iframe-u-ls1"]],
    [["localStorage", "https://sectest1.example.org"],
      []],
    [["sessionStorage", "http://test1.example.org"],
      ["ss1"]],
    [["sessionStorage", "http://sectest1.example.org"],
      ["iframe-u-ss1", "iframe-u-ss2"]],
    [["sessionStorage", "https://sectest1.example.org"],
      []],
    [["indexedDB", "http://test1.example.org", "idb1", "obj1"],
      []],
  ];

  yield checkState(afterState);

  yield finishTests();
});
