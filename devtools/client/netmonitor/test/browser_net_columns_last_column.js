/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that last visible column can't be hidden
 */

add_task(async function() {
  const { monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, parent } = monitor.panelWin;

  const initialColumns = store.getState().ui.columns;
  for (const column in initialColumns) {
    const shown = initialColumns[column];

    const columns = store.getState().ui.columns;
    const visibleColumns = [];
    for (const c in columns) {
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

    const menuItem =
      parent.document.querySelector(`#request-list-header-${column}-toggle`);
    ok(menuItem.disabled, "Last visible column menu item should be disabled.");
  }
});
