/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that last visible column can't be hidden
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, store, parent } = monitor.panelWin;

  for (let [column, shown] of store.getState().ui.columns) {
    let visibleColumns = [...store.getState().ui.columns]
      .filter(([_, visible]) => visible);

    if (visibleColumns.length === 1) {
      yield testLastMenuItem(column);
      break;
    }

    if (shown) {
      yield hideColumn(column);
    }
  }

  yield teardown(monitor);

  function* hideColumn(column) {
    info(`Clicking context-menu item for ${column}`);
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelector("#requests-list-status-button") ||
      document.querySelector("#requests-list-waterfall-button"));

    let onHeaderRemoved = waitForDOM(document, `#requests-list-${column}-button`, 0);
    parent.document.querySelector(`#request-list-header-${column}-toggle`).click();

    yield onHeaderRemoved;
    ok(!document.querySelector(`#requests-list-${column}-button`),
       `Column ${column} should be hidden`);
  }

  function* testLastMenuItem(column) {
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelector(`#requests-list-${column}-button`));

    let menuItem = parent.document.querySelector(`#request-list-header-${column}-toggle`);
    ok(menuItem.disabled, "Last visible column menu item should be disabled.");
  }
});
