/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Test deleting storage items

const TEST_CASES = [
  [["localStorage", "http://test1.example.org"],
    "ls1", "name"],
  [["sessionStorage", "http://test1.example.org"],
    "ss1", "name"],
  [["cookies", "test1.example.org"],
    "c1", "name"],
  [["indexedDB", "http://test1.example.org", "idb1", "obj1"],
    1, "name"]
];

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  let contextMenu = gPanelWindow.document.getElementById("storage-table-popup");
  let menuDeleteItem = contextMenu.querySelector("#storage-table-popup-delete");

  for (let [ treeItem, rowName, cellToClick] of TEST_CASES) {
    let treeItemName = treeItem.join(" > ");

    info(`Selecting tree item ${treeItemName}`);
    yield selectTreeItem(treeItem);

    let row = getRowCells(rowName);
    ok(gUI.table.items.has(rowName), `There is a row '${rowName}' in ${treeItemName}`);

    let eventWait = gUI.once("store-objects-updated");

    yield waitForContextMenu(contextMenu, row[cellToClick], () => {
      info(`Opened context menu in ${treeItemName}, row '${rowName}'`);
      menuDeleteItem.click();
      ok(menuDeleteItem.getAttribute("label").includes(rowName),
        `Context menu item label contains '${rowName}'`);
    });

    yield eventWait;

    ok(!gUI.table.items.has(rowName),
      `There is no row '${rowName}' in ${treeItemName} after deletion`);
  }

  yield finishTests();
});
