/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Test deleting a Cache object from the tree using context menu

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  let contextMenu = gPanelWindow.document.getElementById("storage-tree-popup");
  let menuDeleteItem = contextMenu.querySelector("#storage-tree-popup-delete");

  let cacheToDelete = ["Cache", "http://test1.example.org", "plop"];

  info("test state before delete");
  yield selectTreeItem(cacheToDelete);
  ok(gUI.tree.isSelected(cacheToDelete), "Cache item is present in the tree");

  info("do the delete");
  let eventWait = gUI.once("store-objects-updated");

  let selector = `[data-id='${JSON.stringify(cacheToDelete)}'] > .tree-widget-item`;
  let target = gPanelWindow.document.querySelector(selector);
  ok(target, "Cache item's tree element is present");

  yield waitForContextMenu(contextMenu, target, () => {
    info("Opened tree context menu");
    menuDeleteItem.click();

    let cacheName = cacheToDelete[2];
    ok(menuDeleteItem.getAttribute("label").includes(cacheName),
      `Context menu item label contains '${cacheName}')`);
  });

  yield eventWait;

  info("test state after delete");
  yield selectTreeItem(cacheToDelete);
  ok(!gUI.tree.isSelected(cacheToDelete), "Cache item is no longer present in the tree");

  yield finishTests();
});
