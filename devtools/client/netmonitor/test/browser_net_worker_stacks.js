/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we get stack traces for the network requests made when starting or
// running worker threads.

const TOP_FILE_NAME = "html_worker-test-page.html";
const TOP_URL = EXAMPLE_URL + TOP_FILE_NAME;
const WORKER_FILE_NAME = "js_worker-test.js";
const WORKER_URL = EXAMPLE_URL + WORKER_FILE_NAME;

const EXPECTED_REQUESTS = [
  {
    method: "GET",
    url: TOP_URL,
    causeType: "document",
    causeUri: null,
    stack: false,
  },
  {
    method: "GET",
    url: WORKER_URL,
    causeType: "script",
    causeUri: TOP_URL,
    stack: [
      { fn: "startWorkerInner", file: TOP_URL, line: 11 },
      { fn: "startWorker", file: TOP_URL, line: 8 },
      { file: TOP_URL, line: 4 },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "missing1.js",
    causeType: "script",
    causeUri: TOP_URL,
    stack: [
      { fn: "importScriptsFromWorker", file: WORKER_URL, line: 14 },
      { file: WORKER_URL, line: 10 },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "missing2.js",
    causeType: "script",
    causeUri: TOP_URL,
    stack: [
      { fn: "importScriptsFromWorker", file: WORKER_URL, line: 14 },
      { file: WORKER_URL, line: 10 },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "js_worker-test2.js",
    causeType: "script",
    causeUri: TOP_URL,
    stack: [
      { fn: "startWorkerFromWorker", file: WORKER_URL, line: 7 },
      { file: WORKER_URL, line: 3 },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "missing.json",
    causeType: "xhr",
    causeUri: TOP_URL,
    stack: [
      { fn: "createJSONRequest", file: WORKER_URL, line: 22 },
      { file: WORKER_URL, line: 18 },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "missing.txt",
    causeType: "fetch",
    causeUri: TOP_URL,
    stack: [
      { fn: "fetchThing", file: WORKER_URL, line: 29 },
      { file: WORKER_URL, line: 26 },
    ],
  },
];

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
    EXPECTED_REQUESTS.length
  );
  await navigateTo(TOP_URL);
  await onNetworkEvents;

  is(
    store.getState().requests.requests.length,
    EXPECTED_REQUESTS.length,
    "All the page events should be recorded."
  );

  // Wait for stack traces from all requests.
  const requests = getSortedRequests(store.getState());
  await Promise.all(
    requests.map(requestItem =>
      connector.requestData(requestItem.id, "stackTrace")
    )
  );

  validateRequests(EXPECTED_REQUESTS, monitor);

  await teardown(monitor);
});
