/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Test deleting indexedDB database from the tree using context menu

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-empty-objectstores.html");

  let contextMenu = gPanelWindow.document.getElementById("storage-tree-popup");
  let menuDeleteDb = contextMenu.querySelector(
    "#storage-tree-popup-delete-database");

  info("test state before delete");
  yield checkState([
    [["indexedDB", "http://test1.example.org"], ["idb1", "idb2"]],
  ]);

  info("do the delete");
  const deletedDb = ["indexedDB", "http://test1.example.org", "idb1"];

  yield selectTreeItem(deletedDb);

  // Wait once for update and another time for value fetching
  let eventWait = gUI.once("store-objects-updated").then(
    () => gUI.once("store-objects-updated"));

  let selector = `[data-id='${JSON.stringify(deletedDb)}'] > .tree-widget-item`;
  let target = gPanelWindow.document.querySelector(selector);
  ok(target, `tree item found in ${deletedDb.join(" > ")}`);
  yield waitForContextMenu(contextMenu, target, () => {
    info(`Opened tree context menu in ${deletedDb.join(" > ")}`);
    menuDeleteDb.click();
  });

  yield eventWait;

  info("test state after delete");
  yield checkState([
    [["indexedDB", "http://test1.example.org"], ["idb2"]],
  ]);

  yield finishTests();
});
