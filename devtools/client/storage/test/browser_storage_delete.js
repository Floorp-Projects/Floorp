/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Test deleting storage items

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

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  const contextMenu = gPanelWindow.document.getElementById("storage-table-popup");
  const menuDeleteItem = contextMenu.querySelector("#storage-table-popup-delete");

  for (const [ treeItem, rowName, cellToClick] of TEST_CASES) {
    const treeItemName = treeItem.join(" > ");

    info(`Selecting tree item ${treeItemName}`);
    await selectTreeItem(treeItem);

    const row = getRowCells(rowName);
    ok(gUI.table.items.has(rowName), `There is a row '${rowName}' in ${treeItemName}`);

    const eventWait = gUI.once("store-objects-edit");

    await waitForContextMenu(contextMenu, row[cellToClick], () => {
      info(`Opened context menu in ${treeItemName}, row '${rowName}'`);
      menuDeleteItem.click();
      const truncatedRowName = String(rowName).replace(SEPARATOR_GUID, "-").substr(0, 16);
      ok(menuDeleteItem.getAttribute("label").includes(truncatedRowName),
        `Context menu item label contains '${rowName}' (maybe truncated)`);
    });

    await eventWait;

    ok(!gUI.table.items.has(rowName),
      `There is no row '${rowName}' in ${treeItemName} after deletion`);
  }

  await finishTests();
});
