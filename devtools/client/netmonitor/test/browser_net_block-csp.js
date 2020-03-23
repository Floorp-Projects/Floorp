/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that CSP violations display in the netmonitor when blocked
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CSP_URL, { requestCount: 3 });

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 3);
  tab.linkedBrowser.reload();
  await wait;

  info("Waiting until the requests appear in netmonitor");

  // Ensure the attempt to load a JS file shows a blocked CSP error
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[1],
    "GET",
    EXAMPLE_URL + "js_websocket-worker-test.js",
    {
      transferred: "CSP Preload",
      cause: { type: "script" },
      type: "",
    }
  );

  await teardown(monitor);
});
