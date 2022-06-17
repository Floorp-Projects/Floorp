/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test endless scrolling when a lot of items are present in the storage
// inspector table.
"use strict";

const ITEMS_PER_PAGE = 50;

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN_SECURED + "storage-overflow.html");

  info("Run the tests with short DevTools");
  await runTests();

  info("Close Toolbox");
  await gDevTools.closeToolboxForTab(gBrowser.selectedTab);

  info("Set a toolbox height of 1000px");
  await pushPref("devtools.toolbox.footer.height", 1000);

  info("Open storage panel again");
  await openStoragePanel();

  info("Run the tests with tall DevTools");
  await runTests(true);
});

async function runTests(tall) {
  if (tall) {
    // We need to zoom out and a tall storage panel in order to fit more than 50
    // items in the table. We do this to ensure that we load enough content to
    // show a scrollbar so that we can still use infinite scrolling.
    zoom(0.5);
  }

  gUI.tree.expandAll();
  await selectTreeItem(["localStorage", "https://test1.example.org"]);

  if (tall) {
    if (getCellLength() === ITEMS_PER_PAGE) {
      await scrollToAddItems();
      await waitForStorageData("item-100", "value-100");
    }

    if (getCellLength() === ITEMS_PER_PAGE * 2) {
      await scrollToAddItems();
      await waitForStorageData("item-150", "value-150");
    }

    if (getCellLength() === ITEMS_PER_PAGE * 3) {
      await scrollToAddItems();
      await waitForStorageData("item-151", "value-151");
    }
  } else {
    checkCellLength(ITEMS_PER_PAGE);
    await scrollToAddItems();
    await waitForStorageData("item-100", "value-100");

    checkCellLength(ITEMS_PER_PAGE * 2);
    await scrollToAddItems();
    await waitForStorageData("item-150", "value-150");

    checkCellLength(ITEMS_PER_PAGE * 3);
    await scrollToAddItems();
    await waitForStorageData("item-151", "value-151");
  }

  is(getCellLength(), 151, "Storage table contains 151 items");

  // Check that the columns are sorted in a human readable way (ascending).
  checkCellValues("ASC");

  // Sort descending.
  clickColumnHeader("name");

  // Check that the columns are sorted in a human readable way (descending).
  checkCellValues("DEC");

  if (tall) {
    zoom(1);
  }
}

function checkCellValues(order) {
  const cells = [
    ...gPanelWindow.document.querySelectorAll("#name .table-widget-cell"),
  ];
  cells.forEach(function(cell, index, arr) {
    const i = order === "ASC" ? index + 1 : arr.length - index;
    is(cell.value, `item-${i}`, `Cell value is "item-${i}" (${order}).`);
  });
}

async function scrollToAddItems() {
  info(`Scrolling to add ${ITEMS_PER_PAGE} items`);
  await scroll();
}

function zoom(zoomValue) {
  const bc = BrowsingContext.getFromWindow(gPanelWindow);
  bc.fullZoom = zoomValue;
}
