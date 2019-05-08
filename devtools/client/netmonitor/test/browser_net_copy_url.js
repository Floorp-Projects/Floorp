/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying a request's url works.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  // Execute requests.
  await performRequests(monitor, tab, 1);

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[0]);

  const requestItem = getSortedRequests(store.getState()).get(0);

  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "request-list-context-copy-url").click();
  }, requestItem.url);

  await teardown(monitor);
});
