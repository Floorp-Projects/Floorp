/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Test deleting all storage items

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  const contextMenu = gPanelWindow.document.getElementById("storage-table-popup");
  const menuDeleteAllItem = contextMenu.querySelector(
    "#storage-table-popup-delete-all");

  info("test state before delete");
  const beforeState = [
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
    [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"],
      [1, 2, 3]],
    [["Cache", "http://test1.example.org", "plop"],
      [MAIN_DOMAIN + "404_cached_file.js", MAIN_DOMAIN + "browser_storage_basic.js"]],
  ];

  await checkState(beforeState);

  info("do the delete");
  const deleteHosts = [
    [["localStorage", "https://sectest1.example.org"], "iframe-s-ls1", "name"],
    [["sessionStorage", "https://sectest1.example.org"], "iframe-s-ss1", "name"],
    [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"], 1, "name"],
    [["Cache", "http://test1.example.org", "plop"],
     MAIN_DOMAIN + "404_cached_file.js", "url"],
  ];

  for (const [store, rowName, cellToClick] of deleteHosts) {
    const storeName = store.join(" > ");

    await selectTreeItem(store);

    const eventWait = gUI.once("store-objects-cleared");

    const cell = getRowCells(rowName)[cellToClick];
    await waitForContextMenu(contextMenu, cell, () => {
      info(`Opened context menu in ${storeName}, row '${rowName}'`);
      menuDeleteAllItem.click();
    });

    await eventWait;
  }

  info("test state after delete");
  const afterState = [
    // iframes from the same host, one secure, one unsecure, are independent
    // from each other. Delete all in one doesn't touch the other one.
    [["localStorage", "http://test1.example.org"],
      ["key", "ls1", "ls2"]],
    [["localStorage", "http://sectest1.example.org"],
      ["iframe-u-ls1"]],
    [["localStorage", "https://sectest1.example.org"],
      []],
    [["sessionStorage", "http://test1.example.org"],
      ["key", "ss1"]],
    [["sessionStorage", "http://sectest1.example.org"],
      ["iframe-u-ss1", "iframe-u-ss2"]],
    [["sessionStorage", "https://sectest1.example.org"],
      []],
    [["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"],
      []],
    [["Cache", "http://test1.example.org", "plop"],
      []],
  ];

  await checkState(afterState);

  await finishTests();
});
