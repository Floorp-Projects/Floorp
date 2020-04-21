/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const {
  getUrlBaseName,
} = require("devtools/client/netmonitor/src/utils/request-utils");
/**
 * Tests if request initiator is reported correctly.
 */

const INITIATOR_FILE_NAME = "html_cause-test-page.html";
const INITIATOR_URL = EXAMPLE_URL + INITIATOR_FILE_NAME;

const EXPECTED_REQUESTS = [
  {
    method: "GET",
    url: INITIATOR_URL,
    causeType: "document",
    causeUri: null,
    stack: false,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "stylesheet_request",
    causeType: "stylesheet",
    causeUri: INITIATOR_URL,
    stack: false,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "img_request",
    causeType: "img",
    causeUri: INITIATOR_URL,
    stack: false,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "img_srcset_request",
    causeType: "imageset",
    causeUri: INITIATOR_URL,
    stack: false,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "xhr_request",
    causeType: "xhr",
    causeUri: INITIATOR_URL,
    stack: [{ fn: "performXhrRequestCallback", file: INITIATOR_URL, line: 30 }],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "fetch_request",
    causeType: "fetch",
    causeUri: INITIATOR_URL,
    stack: [{ fn: "performFetchRequest", file: INITIATOR_URL, line: 35 }],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "promise_fetch_request",
    causeType: "fetch",
    causeUri: INITIATOR_URL,
    stack: [
      {
        fn: "performPromiseFetchRequestCallback",
        file: INITIATOR_URL,
        line: 41,
      },
      {
        fn: "performPromiseFetchRequest",
        file: INITIATOR_URL,
        line: 40,
        asyncCause: "promise callback",
      },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "timeout_fetch_request",
    causeType: "fetch",
    causeUri: INITIATOR_URL,
    stack: [
      {
        fn: "performTimeoutFetchRequestCallback2",
        file: INITIATOR_URL,
        line: 48,
      },
      {
        fn: "performTimeoutFetchRequestCallback1",
        file: INITIATOR_URL,
        line: 47,
        asyncCause: "setTimeout handler",
      },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "lazy_img_request",
    causeType: "lazy-img",
    causeUri: INITIATOR_URL,
    stack: false,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "lazy_img_srcset_request",
    causeType: "lazy-imageset",
    causeUri: INITIATOR_URL,
    stack: false,
  },
  {
    method: "POST",
    url: EXAMPLE_URL + "beacon_request",
    causeType: "beacon",
    causeUri: INITIATOR_URL,
    stack: [{ fn: "performBeaconRequest", file: INITIATOR_URL, line: 70 }],
  },
];

add_task(async function() {
  // Async stacks aren't on by default in all builds
  await SpecialPowers.pushPrefEnv({
    set: [["javascript.options.asyncstack", true]],
  });

  // the initNetMonitor function clears the network request list after the
  // page is loaded. That's why we first load a bogus page from SIMPLE_URL,
  // and only then load the real thing from INITIATOR_URL - we want to catch
  // all the requests the page is making, not only the XHRs.
  // We can't use about:blank here, because initNetMonitor checks that the
  // page has actually made at least one request.
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, EXPECTED_REQUESTS.length);
  BrowserTestUtils.loadURI(tab.linkedBrowser, INITIATOR_URL);
  await wait;

  // For all expected requests
  for (const [index, { stack }] of EXPECTED_REQUESTS.entries()) {
    if (!stack) {
      continue;
    }

    EventUtils.sendMouseEvent(
      { type: "mousedown" },
      document.querySelectorAll(
        ".request-list-item .requests-list-initiator-lastframe"
      )[index]
    );

    // Clicking on the initiator column should open the Stack Trace panel
    const onStackTraceRendered = waitUntil(() =>
      document.querySelector("#stack-trace-panel .stack-trace .frame-link")
    );
    await onStackTraceRendered;
  }

  is(
    store.getState().requests.requests.length,
    EXPECTED_REQUESTS.length,
    "All the page events should be recorded."
  );

  validateRequests(EXPECTED_REQUESTS, monitor);

  // Sort the requests by initiator and check the order
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#requests-list-initiator-button")
  );

  const expectedOrder = EXPECTED_REQUESTS.sort(initiatorSortPredicate).map(
    r => {
      const lastFrameExists = !!r.stack;
      let initiator = "";
      let lineNumber = "";
      if (lastFrameExists) {
        const { file, line: _lineNumber } = r.stack[0];
        initiator = getUrlBaseName(file);
        lineNumber = ":" + _lineNumber;
      }
      const causeStr = lastFrameExists ? " (" + r.causeType + ")" : r.causeType;
      return initiator + lineNumber + causeStr;
    }
  );

  expectedOrder.forEach((expectedInitiator, i) => {
    const request = getSortedRequests(store.getState())[i];
    let initiator;
    if (request.cause.stacktraceAvailable) {
      const { filename, lineNumber } = request.cause.lastFrame;
      initiator =
        getUrlBaseName(filename) +
        ":" +
        lineNumber +
        " (" +
        request.cause.type +
        ")";
    } else {
      initiator = request.cause.type;
    }
    is(
      initiator,
      expectedInitiator,
      `The request #${i} has the expected initiator after sorting`
    );
  });

  await teardown(monitor);
});

// derived from devtools/client/netmonitor/src/utils/sort-predicates.js
function initiatorSortPredicate(first, second) {
  const firstLastFrame = first.stack ? first.stack[0] : null;
  const secondLastFrame = second.stack ? second.stack[0] : null;

  let firstInitiator = "";
  let firstInitiatorLineNumber = 0;

  if (firstLastFrame) {
    firstInitiator = getUrlBaseName(firstLastFrame.file);
    firstInitiatorLineNumber = firstLastFrame.line;
  }

  let secondInitiator = "";
  let secondInitiatorLineNumber = 0;

  if (secondLastFrame) {
    secondInitiator = getUrlBaseName(secondLastFrame.file);
    secondInitiatorLineNumber = secondLastFrame.line;
  }

  let result;
  // if both initiators don't have a stack trace, compare their causes
  if (!firstInitiator && !secondInitiator) {
    result = compareValues(first.causeType, second.causeType);
  } else if (!firstInitiator || !secondInitiator) {
    // if one initiator doesn't have a stack trace but the other does, former should precede the latter
    result = compareValues(firstInitiatorLineNumber, secondInitiatorLineNumber);
  } else {
    result = compareValues(firstInitiator, secondInitiator);
    if (result === 0) {
      result = compareValues(
        firstInitiatorLineNumber,
        secondInitiatorLineNumber
      );
    }
  }
  return result;
}
