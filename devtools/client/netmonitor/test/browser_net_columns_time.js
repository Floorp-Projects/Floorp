/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for timings columns.
 */
add_task(async function () {
  let { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  hideColumn(monitor, "waterfall");
  showColumn(monitor, "endTime");
  showColumn(monitor, "responseTime");
  showColumn(monitor, "duration");
  showColumn(monitor, "latency");

  let onNetworkEvents = waitForNetworkEvents(monitor, 1);
  let onEventTimings = waitFor(monitor.panelWin, EVENTS.RECEIVED_EVENT_TIMINGS);
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
