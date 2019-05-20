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
    stack: true,
  },
  {
    method: "GET",
    url: WORKER_URL,
    causeType: "script",
    causeUri: TOP_URL,
    stack: [
      { fn: "startWorkerInner", file: TOP_FILE_NAME, line: 11 },
      { fn: "startWorker", file: TOP_FILE_NAME, line: 8 },
      { file: TOP_FILE_NAME, line: 4 },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "missing1.js",
    causeType: "script",
    causeUri: TOP_URL,
    stack: [
      { fn: "importScriptsFromWorker", file: WORKER_FILE_NAME, line: 14 },
      { file: WORKER_FILE_NAME, line: 10 },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "missing2.js",
    causeType: "script",
    causeUri: TOP_URL,
    stack: [
      { fn: "importScriptsFromWorker", file: WORKER_FILE_NAME, line: 14 },
      { file: WORKER_FILE_NAME, line: 10 },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "js_worker-test2.js",
    causeType: "script",
    causeUri: TOP_URL,
    stack: [
      { fn: "startWorkerFromWorker", file: WORKER_FILE_NAME, line: 7 },
      { file: WORKER_FILE_NAME, line: 3 },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "missing.json",
    causeType: "xhr",
    causeUri: TOP_URL,
    stack: [
      { fn: "createJSONRequest", file: WORKER_FILE_NAME, line: 22 },
      { file: WORKER_FILE_NAME, line: 18 },
    ],
  },
];

add_task(async function() {
  // Load a different URL first to instantiate the network monitor before we
  // load the page we're really interested in.
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);

  const { store, windowRequire, connector } = monitor.panelWin;
  const {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  BrowserTestUtils.loadURI(tab.linkedBrowser, TOP_URL);

  await waitForNetworkEvents(monitor, EXPECTED_REQUESTS.length);

  is(store.getState().requests.requests.size, EXPECTED_REQUESTS.length,
    "All the page events should be recorded.");

  // Wait for stack traces from all requests.
  const requests = getSortedRequests(store.getState());
  await Promise.all(requests.map(requestItem =>
    connector.requestData(requestItem.id, "stackTrace")));

  validateRequests(EXPECTED_REQUESTS, monitor);

  await teardown(monitor);
});
