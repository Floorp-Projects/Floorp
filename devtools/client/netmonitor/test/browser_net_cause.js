/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if request cause is reported correctly.
 */

const CAUSE_FILE_NAME = "html_cause-test-page.html";
const CAUSE_URL = EXAMPLE_URL + CAUSE_FILE_NAME;

const EXPECTED_REQUESTS = [
  {
    method: "GET",
    url: CAUSE_URL,
    causeType: "document",
    causeUri: null,
    stack: false,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "stylesheet_request",
    causeType: "stylesheet",
    causeUri: CAUSE_URL,
    stack: false,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "img_request",
    causeType: "img",
    causeUri: CAUSE_URL,
    stack: false,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "xhr_request",
    causeType: "xhr",
    causeUri: CAUSE_URL,
    stack: [
      { fn: "performXhrRequestCallback", file: CAUSE_FILE_NAME, line: 28 },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "fetch_request",
    causeType: "fetch",
    causeUri: CAUSE_URL,
    stack: [{ fn: "performFetchRequest", file: CAUSE_FILE_NAME, line: 33 }],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "promise_fetch_request",
    causeType: "fetch",
    causeUri: CAUSE_URL,
    stack: [
      {
        fn: "performPromiseFetchRequestCallback",
        file: CAUSE_FILE_NAME,
        line: 39,
      },
      {
        fn: "performPromiseFetchRequest",
        file: CAUSE_FILE_NAME,
        line: 38,
        asyncCause: "promise callback",
      },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "timeout_fetch_request",
    causeType: "fetch",
    causeUri: CAUSE_URL,
    stack: [
      {
        fn: "performTimeoutFetchRequestCallback2",
        file: CAUSE_FILE_NAME,
        line: 46,
      },
      {
        fn: "performTimeoutFetchRequestCallback1",
        file: CAUSE_FILE_NAME,
        line: 45,
        asyncCause: "setTimeout handler",
      },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "lazy_img_request",
    causeType: "lazy-img",
    causeUri: CAUSE_URL,
    stack: false,
  },
  {
    method: "POST",
    url: EXAMPLE_URL + "beacon_request",
    causeType: "beacon",
    causeUri: CAUSE_URL,
    stack: [{ fn: "performBeaconRequest", file: CAUSE_FILE_NAME, line: 60 }],
  },
];

add_task(async function() {
  // Async stacks aren't on by default in all builds
  await SpecialPowers.pushPrefEnv({
    set: [["javascript.options.asyncstack", true]],
  });

  // the initNetMonitor function clears the network request list after the
  // page is loaded. That's why we first load a bogus page from SIMPLE_URL,
  // and only then load the real thing from CAUSE_URL - we want to catch
  // all the requests the page is making, not only the XHRs.
  // We can't use about:blank here, because initNetMonitor checks that the
  // page has actually made at least one request.
  const { monitor } = await initNetMonitor(SIMPLE_URL, { requestCount: 1 });

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, EXPECTED_REQUESTS.length);
  await navigateTo(CAUSE_URL);
  await wait;

  // For all expected requests
  for (const [index, { stack }] of EXPECTED_REQUESTS.entries()) {
    if (!stack) {
      continue;
    }

    // Select them
    EventUtils.sendMouseEvent(
      { type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]
    );

    // And select the stack trace panel
    const onStackTraceRendered = waitUntil(() =>
      document.querySelector("#stack-trace-panel .stack-trace .frame-link")
    );
    document.querySelector("#stack-trace-tab").click();
    await onStackTraceRendered;
  }

  is(
    store.getState().requests.requests.length,
    EXPECTED_REQUESTS.length,
    "All the page events should be recorded."
  );

  validateRequests(EXPECTED_REQUESTS, monitor);

  // Sort the requests by cause and check the order
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#requests-list-cause-button")
  );
  const expectedOrder = EXPECTED_REQUESTS.map(r => r.causeType).sort();
  expectedOrder.forEach((expectedCause, i) => {
    const cause = getSortedRequests(store.getState())[i].cause.type;
    is(
      cause,
      expectedCause,
      `The request #${i} has the expected cause after sorting`
    );
  });

  await teardown(monitor);
});
