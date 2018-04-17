/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for timings columns.
 */
add_task(async function() {
  let { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  let visibleColumns = store.getState().ui.columns;

  // Hide the waterfall column to make sure timing data are fetched
  // by the other timing columns ("endTime", "responseTime", "duration",
  // "latency").
  // Note that all these timing columns are based on the same
  // `RequestListColumnTime` component.
  if (visibleColumns.waterfall) {
    await hideColumn(monitor, "waterfall");
  }

  ["endTime", "responseTime", "duration", "latency"].forEach(async (column) => {
    if (!visibleColumns[column]) {
      await showColumn(monitor, column);
    }
  });

  let onNetworkEvents = waitForNetworkEvents(monitor, 1);
  let onEventTimings = waitFor(monitor.panelWin.api, EVENTS.RECEIVED_EVENT_TIMINGS);
  tab.linkedBrowser.reload();
  await Promise.all([onNetworkEvents, onEventTimings]);

  // There should be one request in the list.
  let requestItems = document.querySelectorAll(".request-list-item");
  is(requestItems.length, 1, "There must be one visible item");

  let item = requestItems[0];
  let types = [
    "end",
    "response",
    "duration",
    "latency"
  ];

  for (let t of types) {
    await waitUntil(() => {
      let node = item.querySelector(".requests-list-" + t + "-time");
      let value = parseInt(node.textContent, 10);
      return value > 0;
    });
  }

  await teardown(monitor);
});
