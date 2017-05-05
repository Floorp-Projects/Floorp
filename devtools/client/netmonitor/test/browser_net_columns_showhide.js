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

  for (let [column, shown] of store.getState().ui.columns) {
    if (shown) {
      yield testVisibleColumnContextMenuItem(column, document, parent);
      yield testHiddenColumnContextMenuItem(column, document, parent);
    } else {
      yield testHiddenColumnContextMenuItem(column, document, parent);
      yield testVisibleColumnContextMenuItem(column, document, parent);
    }
  }
});

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
