/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we get stack traces for the network requests made when creating
// web sockets on the main or worker threads.

const TOP_FILE_NAME = "html_websocket-test-page.html";
const TOP_URL = HTTPS_EXAMPLE_URL + TOP_FILE_NAME;
const WORKER_FILE_NAME = "js_websocket-worker-test.js";

const EXPECTED_REQUESTS = {
  [TOP_URL]: {
    method: "GET",
    url: TOP_URL,
    causeType: "document",
    causeUri: null,
    stack: false,
  },
  "ws://localhost:8080/": {
    method: "GET",
    url: "ws://localhost:8080/",
    causeType: "websocket",
    causeUri: TOP_URL,
    stack: [
      { fn: "openSocket", file: TOP_URL, line: 6 },
      { file: TOP_FILE_NAME, line: 3 },
    ],
  },
  [HTTPS_EXAMPLE_URL + WORKER_FILE_NAME]: {
    method: "GET",
    url: HTTPS_EXAMPLE_URL + WORKER_FILE_NAME,
    causeType: "script",
    causeUri: TOP_URL,
    stack: [{ file: TOP_URL, line: 9 }],
  },
  "wss://localhost:8081/": {
    method: "GET",
    url: "wss://localhost:8081/",
    causeType: "websocket",
    causeUri: TOP_URL,
    stack: [
      {
        fn: "openWorkerSocket",
        file: HTTPS_EXAMPLE_URL + WORKER_FILE_NAME,
        line: 5,
      },
      { file: WORKER_FILE_NAME, line: 2 },
    ],
  },
};

add_task(async function() {
  // Load a different URL first to instantiate the network monitor before we
  // load the page we're really interested in.
  const { monitor } = await initNetMonitor(SIMPLE_URL, { requestCount: 1 });

  const { store, windowRequire, connector } = monitor.panelWin;
  const { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  const onNetworkEvents = waitForNetworkEvents(
    monitor,
    Object.keys(EXPECTED_REQUESTS).length
  );
  await navigateTo(TOP_URL);
  await onNetworkEvents;

  is(
    store.getState().requests.requests.length,
    Object.keys(EXPECTED_REQUESTS).length,
    "All the page events should be recorded."
  );

  const requests = getSortedRequests(store.getState());
  // The expected requests in the same order as the
  // requests. The platform does not guarantee the order so the
  // tests should not depend on that.
  const expectedRequests = [];

  // Wait for stack traces from all requests.
  await Promise.all(
    requests.map(request => {
      expectedRequests.push(EXPECTED_REQUESTS[request.url]);
      return connector.requestData(request.id, "stackTrace");
    })
  );

  validateRequests(expectedRequests, monitor);

  await teardown(monitor);
});
