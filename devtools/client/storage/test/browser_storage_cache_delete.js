/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Test deleting a Cache object from the tree using context menu

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  const contextMenu = gPanelWindow.document.getElementById("storage-tree-popup");
  const menuDeleteItem = contextMenu.querySelector("#storage-tree-popup-delete");

  const cacheToDelete = ["Cache", "http://test1.example.org", "plop"];

  info("test state before delete");
  await selectTreeItem(cacheToDelete);
  ok(gUI.tree.isSelected(cacheToDelete), "Cache item is present in the tree");

  info("do the delete");
  const eventWait = gUI.once("store-objects-updated");

  const selector = `[data-id='${JSON.stringify(cacheToDelete)}'] > .tree-widget-item`;
  const target = gPanelWindow.document.querySelector(selector);
  ok(target, "Cache item's tree element is present");

  await waitForContextMenu(contextMenu, target, () => {
    info("Opened tree context menu");
    menuDeleteItem.click();

    const cacheName = cacheToDelete[2];
    ok(menuDeleteItem.getAttribute("label").includes(cacheName),
      `Context menu item label contains '${cacheName}')`);
  });

  await eventWait;

  info("test state after delete");
  await selectTreeItem(cacheToDelete);
  ok(!gUI.tree.isSelected(cacheToDelete), "Cache item is no longer present in the tree");

  await finishTests();
});
