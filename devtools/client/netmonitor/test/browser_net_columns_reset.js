/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests reset column menu item. Note that the column
 * header is visible only if there are requests in the list.
 */
add_task(async function() {
  const { monitor, tab } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const { Prefs } = windowRequire("devtools/client/netmonitor/src/utils/prefs");

  const prefBefore = Prefs.visibleColumns;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await wait;

  await hideColumn(monitor, "status");
  await hideColumn(monitor, "waterfall");

  EventUtils.sendMouseEvent(
    { type: "contextmenu" },
    document.querySelector("#requests-list-contentSize-button")
  );

  getContextMenuItem(monitor, "request-list-header-reset-columns").click();

  ok(
    JSON.stringify(prefBefore) === JSON.stringify(Prefs.visibleColumns),
    "Reset columns item should reset columns pref"
  );

  return teardown(monitor);
});
