/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test endless scrolling when a lot of items are present in the storage
// inspector table.
"use strict";

const ITEMS_PER_PAGE = 50;

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-overflow.html");

  info("Run the tests with short DevTools");
  await runTests();

  info("Close Toolbox");
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  await gDevTools.closeToolbox(target);

  info("Set a toolbox height of 1000px");
  await pushPref("devtools.toolbox.footer.height", 1000);

  info("Open storage panel again");
  await openStoragePanel();

  info("Run the tests with tall DevTools");
  await runTests();

  await finishTests();
});

async function runTests() {
  gUI.tree.expandAll();
  await selectTreeItem(["localStorage", "http://test1.example.org"]);
  checkCellLength(ITEMS_PER_PAGE);

  await scroll();
  checkCellLength(ITEMS_PER_PAGE * 2);

  await scroll();
  checkCellLength(ITEMS_PER_PAGE * 3);

  // Check that the columns are sorted in a human readable way (ascending).
  checkCellValues("ASC");

  // Sort descending.
  clickColumnHeader("name");

  // Check that the columns are sorted in a human readable way (descending).
  checkCellValues("DEC");
}

function checkCellValues(order) {
  const cells = [...gPanelWindow.document
                              .querySelectorAll("#name .table-widget-cell")];
  cells.forEach(function(cell, index, arr) {
    const i = order === "ASC" ? index + 1 : arr.length - index;
    is(cell.value, `item-${i}`, `Cell value is correct (${order}).`);
  });
}
