/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test deleting storage items

const TEST_CASES = [
  [["localStorage", "http://test1.example.org"], "ls1", "name"],
  [["sessionStorage", "http://test1.example.org"], "ss1", "name"],
  [
    ["cookies", "http://test1.example.org"],
    getCookieId("c1", "test1.example.org", "/browser"),
    "name",
  ],
  [
    ["indexedDB", "http://test1.example.org", "idb1 (default)", "obj1"],
    1,
    "name",
  ],
  [
    ["Cache", "http://test1.example.org", "plop"],
    MAIN_DOMAIN + "404_cached_file.js",
    "url",
  ],
];

add_task(async function () {
  // storage-listings.html explicitly mixes secure and insecure frames.
  // We should not enforce https for tests using this page.
  await pushPref("dom.security.https_first", false);

  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  const contextMenu = gPanelWindow.document.getElementById(
    "storage-table-popup"
  );
  const menuDeleteItem = contextMenu.querySelector(
    "#storage-table-popup-delete"
  );

  for (const [treeItem, rowName, cellToClick] of TEST_CASES) {
    const treeItemName = treeItem.join(" > ");

    info(`Selecting tree item ${treeItemName}`);
    await selectTreeItem(treeItem);

    const row = getRowCells(rowName);
    ok(
      gUI.table.items.has(rowName),
      `There is a row '${rowName}' in ${treeItemName}`
    );

    const eventWait = gUI.once("store-objects-edit");

    await waitForContextMenu(contextMenu, row[cellToClick], () => {
      info(`Opened context menu in ${treeItemName}, row '${rowName}'`);
      contextMenu.activateItem(menuDeleteItem);
      const truncatedRowName = String(rowName)
        .replace(SEPARATOR_GUID, "-")
        .substr(0, 16);
      ok(
        JSON.parse(
          menuDeleteItem.getAttribute("data-l10n-args")
        ).itemName.includes(truncatedRowName),
        `Context menu item label contains '${rowName}' (maybe truncated)`
      );
    });

    info("Awaiting for store-objects-edit event");
    await eventWait;

    ok(
      !gUI.table.items.has(rowName),
      `There is no row '${rowName}' in ${treeItemName} after deletion`
    );
  }
});
