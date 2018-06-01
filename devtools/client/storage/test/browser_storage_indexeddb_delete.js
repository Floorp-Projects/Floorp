/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Test deleting indexedDB database from the tree using context menu

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-empty-objectstores.html");

  const contextMenu = gPanelWindow.document.getElementById("storage-tree-popup");
  const menuDeleteDb = contextMenu.querySelector("#storage-tree-popup-delete");

  info("test state before delete");
  await checkState([
    [["indexedDB", "http://test1.example.org"], ["idb1 (default)", "idb2 (default)"]],
  ]);

  info("do the delete");
  const deletedDb = ["indexedDB", "http://test1.example.org", "idb1 (default)"];

  await selectTreeItem(deletedDb);

  // Wait once for update and another time for value fetching
  const eventWait = gUI.once("store-objects-updated");

  const selector = `[data-id='${JSON.stringify(deletedDb)}'] > .tree-widget-item`;
  const target = gPanelWindow.document.querySelector(selector);
  ok(target, `tree item found in ${deletedDb.join(" > ")}`);
  await waitForContextMenu(contextMenu, target, () => {
    info(`Opened tree context menu in ${deletedDb.join(" > ")}`);
    menuDeleteDb.click();
  });

  await eventWait;

  info("test state after delete");
  await checkState([
    [["indexedDB", "http://test1.example.org"], ["idb2 (default)"]],
  ]);

  await finishTests();
});
