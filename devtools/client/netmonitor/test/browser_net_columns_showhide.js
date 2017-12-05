/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test showing/hiding columns.
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, store, parent } = monitor.panelWin;

  let columns = store.getState().ui.columns;
  for (let column in columns) {
    if (columns[column]) {
      yield testVisibleColumnContextMenuItem(column, document, parent);
      yield testHiddenColumnContextMenuItem(column, document, parent);
    } else {
      yield testHiddenColumnContextMenuItem(column, document, parent);
      yield testVisibleColumnContextMenuItem(column, document, parent);
    }
  }

  columns = store.getState().ui.columns;
  for (let column in columns) {
    if (columns[column]) {
      yield testVisibleColumnContextMenuItem(column, document, parent);
      // Right click on the white-space for the context menu to appear
      // and toggle column visibility
      yield testWhiteSpaceContextMenuItem(column, document, parent);
    }
  }
});

function* testWhiteSpaceContextMenuItem(column, document, parent) {
  ok(!document.querySelector(`#requests-list-${column}-button`),
     `Column ${column} should be hidden`);

  info(`Right clicking on white-space in the header to get the context menu`);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelector(".devtools-toolbar.requests-list-headers"));

  yield toggleAndCheckColumnVisibility(column, document, parent);
}

function* testVisibleColumnContextMenuItem(column, document, parent) {
  ok(document.querySelector(`#requests-list-${column}-button`),
     `Column ${column} should be visible`);

  info(`Clicking context-menu item for ${column}`);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelector("#requests-list-status-button") ||
    document.querySelector("#requests-list-waterfall-button"));

  let menuItem = parent.document.querySelector(`#request-list-header-${column}-toggle`);

  is(menuItem.getAttribute("type"), "checkbox",
     `${column} menu item should have type="checkbox" attribute`);
  is(menuItem.getAttribute("checked"), "true",
     `checked state of ${column} menu item should be correct`);
  ok(!menuItem.disabled, `disabled state of ${column} menu item should be correct`);

  let onHeaderRemoved = waitForDOM(document, `#requests-list-${column}-button`, 0);
  menuItem.click();

  yield onHeaderRemoved;

  ok(!document.querySelector(`#requests-list-${column}-button`),
     `Column ${column} should be hidden`);
}

function* testHiddenColumnContextMenuItem(column, document, parent) {
  ok(!document.querySelector(`#requests-list-${column}-button`),
     `Column ${column} should be hidden`);

  info(`Clicking context-menu item for ${column}`);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelector("#requests-list-status-button") ||
    document.querySelector("#requests-list-waterfall-button"));

  yield toggleAndCheckColumnVisibility(column, document, parent);
}

function* toggleAndCheckColumnVisibility(column, document, parent) {
  let menuItem = parent.document.querySelector(`#request-list-header-${column}-toggle`);

  is(menuItem.getAttribute("type"), "checkbox",
     `${column} menu item should have type="checkbox" attribute`);
  ok(!menuItem.getAttribute("checked"),
     `checked state of ${column} menu item should be correct`);
  ok(!menuItem.disabled, `disabled state of ${column} menu item should be correct`);

  let onHeaderAdded = waitForDOM(document, `#requests-list-${column}-button`, 1);
  menuItem.click();

  yield onHeaderAdded;

  ok(document.querySelector(`#requests-list-${column}-button`),
     `Column ${column} should be visible`);
}
