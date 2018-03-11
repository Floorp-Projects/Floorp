// Test endless scrolling when a lot of items are present in the storage
// inspector table.
"use strict";

const ITEMS_PER_PAGE = 50;

add_task(async function () {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-overflow.html");

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

  await finishTests();
});

function checkCellLength(len) {
  let cells = gPanelWindow.document
                          .querySelectorAll("#name .table-widget-cell");
  let msg = `Table should initially display ${len} items`;

  is(cells.length, len, msg);
}

function checkCellValues(order) {
  let cells = [...gPanelWindow.document
                              .querySelectorAll("#name .table-widget-cell")];
  cells.forEach(function (cell, index, arr) {
    let i = order === "ASC" ? index + 1 : arr.length - index;
    is(cell.value, `item-${i}`, `Cell value is correct (${order}).`);
  });
}

async function scroll() {
  let $ = id => gPanelWindow.document.querySelector(id);
  let table = $("#storage-table .table-widget-body");
  let cell = $("#name .table-widget-cell");
  let cellHeight = cell.getBoundingClientRect().height;

  let onStoresUpdate = gUI.once("store-objects-updated");
  table.scrollTop += cellHeight * 50;
  await onStoresUpdate;
}
