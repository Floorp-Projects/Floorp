/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that last visible column can't be hidden
 */

add_task(async function() {
  let { monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, store, parent } = monitor.panelWin;

  let initialColumns = store.getState().ui.columns;
  for (let column in initialColumns) {
    let shown = initialColumns[column];

    let columns = store.getState().ui.columns;
    let visibleColumns = [];
    for (let c in columns) {
      if (columns[c]) {
        visibleColumns.push(c);
      }
    }

    if (visibleColumns.length === 1) {
      if (!shown) {
        continue;
      }
      await testLastMenuItem(column);
      break;
    }

    if (shown) {
      await hideColumn(monitor, column);
    }
  }

  await teardown(monitor);

  async function testLastMenuItem(column) {
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelector(`#requests-list-${column}-button`));

    let menuItem = parent.document.querySelector(`#request-list-header-${column}-toggle`);
    ok(menuItem.disabled, "Last visible column menu item should be disabled.");
  }
});
