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
  const { getDisplayedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 3);
  tab.linkedBrowser.reload();
  info("Waiting until the requests appear in netmonitor");
  await wait;

  const displayedRequests = getDisplayedRequests(store.getState());

  const styleRequest = displayedRequests.find(request =>
    request.url.includes("internal-loaded.css")
  );
  // Ensure the attempt to load a CSS file shows a blocked CSP error
  verifyRequestItemTarget(
    document,
    displayedRequests,
    styleRequest,
    "GET",
    EXAMPLE_URL + "internal-loaded.css",
    {
      transferred: "CSP",
      cause: { type: "stylesheet" },
      type: "",
    }
  );

  const scriptRequest = displayedRequests.find(request =>
    request.url.includes("js_websocket-worker-test.js")
  );
  // Ensure the attempt to load a JS file shows a blocked CSP error
  verifyRequestItemTarget(
    document,
    displayedRequests,
    scriptRequest,
    "GET",
    EXAMPLE_URL + "js_websocket-worker-test.js",
    {
      transferred: "CSP",
      cause: { type: "script" },
      type: "",
    }
  );

  await teardown(monitor);
});
