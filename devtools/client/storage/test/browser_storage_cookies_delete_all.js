/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Test deleting all cookies

function* performDelete(store, rowName, deleteAll) {
  let contextMenu = gPanelWindow.document.getElementById(
    "storage-table-popup");
  let menuDeleteAllItem = contextMenu.querySelector(
    "#storage-table-popup-delete-all");
  let menuDeleteAllFromItem = contextMenu.querySelector(
    "#storage-table-popup-delete-all-from");

  let storeName = store.join(" > ");

  yield selectTreeItem(store);

  let eventWait = gUI.once("store-objects-updated");

  let cells = getRowCells(rowName);
  yield waitForContextMenu(contextMenu, cells.name, () => {
    info(`Opened context menu in ${storeName}, row '${rowName}'`);
    if (deleteAll) {
      menuDeleteAllItem.click();
    } else {
      menuDeleteAllFromItem.click();
      let hostName = cells.host.value;
      ok(menuDeleteAllFromItem.getAttribute("label").includes(hostName),
        `Context menu item label contains '${hostName}'`);
    }
  });

  yield eventWait;
}

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  info("test state before delete");
  yield checkState([
    [["cookies", "test1.example.org"], ["c1", "c3", "cs2", "uc1"]],
    [["cookies", "sectest1.example.org"], ["cs2", "sc1", "uc1"]],
  ]);

  info("delete all from domain");
  // delete only cookies that match the host exactly
  yield performDelete(["cookies", "test1.example.org"], "c1", false);

  info("test state after delete all from domain");
  yield checkState([
    // Domain cookies (.example.org) must not be deleted.
    [["cookies", "test1.example.org"], ["cs2", "uc1"]],
    [["cookies", "sectest1.example.org"], ["cs2", "sc1", "uc1"]],
  ]);

  info("delete all");
  // delete all cookies for host, including domain cookies
  yield performDelete(["cookies", "sectest1.example.org"], "uc1", true);

  info("test state after delete all");
  yield checkState([
    // Domain cookies (.example.org) are deleted too, so deleting in sectest1
    // also removes stuff from test1.
    [["cookies", "test1.example.org"], []],
    [["cookies", "sectest1.example.org"], []],
  ]);

  yield finishTests();
});
