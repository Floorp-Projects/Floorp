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
    causeUri: "",
    // The document load is from JS function in e10s, native in non-e10s
    hasStack: !gMultiProcessBrowser
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "stylesheet_request",
    causeType: "stylesheet",
    causeUri: CAUSE_URL,
    hasStack: false
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "img_request",
    causeType: "img",
    causeUri: CAUSE_URL,
    hasStack: false
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "xhr_request",
    causeType: "xhr",
    causeUri: CAUSE_URL,
    hasStack: { fn: "performXhrRequest", file: CAUSE_FILE_NAME, line: 22 }
  },
  {
    method: "POST",
    url: EXAMPLE_URL + "beacon_request",
    causeType: "beacon",
    causeUri: CAUSE_URL,
    hasStack: { fn: "performBeaconRequest", file: CAUSE_FILE_NAME, line: 26 }
  },
];

var test = Task.async(function* () {
  // the initNetMonitor function clears the network request list after the
  // page is loaded. That's why we first load a bogus page from SIMPLE_URL,
  // and only then load the real thing from CAUSE_URL - we want to catch
  // all the requests the page is making, not only the XHRs.
  // We can't use about:blank here, because initNetMonitor checks that the
  // page has actually made at least one request.
  let [, debuggee, monitor] = yield initNetMonitor(SIMPLE_URL);
  let { RequestsMenu } = monitor.panelWin.NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  debuggee.location = CAUSE_URL;

  yield waitForNetworkEvents(monitor, EXPECTED_REQUESTS.length);

  is(RequestsMenu.itemCount, EXPECTED_REQUESTS.length,
    "All the page events should be recorded.");

  EXPECTED_REQUESTS.forEach((spec, i) => {
    let { method, url, causeType, causeUri, hasStack } = spec;

    let requestItem = RequestsMenu.getItemAtIndex(i);
    verifyRequestItemTarget(requestItem,
      method, url, { cause: { type: causeType, loadingDocumentUri: causeUri } }
    );

    let { stacktrace } = requestItem.attachment.cause;
    let stackLen = stacktrace ? stacktrace.length : 0;

    if (hasStack) {
      ok(stacktrace, `Request #${i} has a stacktrace`);
      ok(stackLen > 0,
        `Request #${i} (${causeType}) has a stacktrace with ${stackLen} items`);

      // if "hasStack" is object, check the details about the top stack frame
      if (typeof hasStack === "object") {
        is(stacktrace[0].functionName, hasStack.fn,
          `Request #${i} has the correct function on top of the JS stack`);
        is(stacktrace[0].filename.split("/").pop(), hasStack.file,
          `Request #${i} has the correct file on top of the JS stack`);
        is(stacktrace[0].lineNumber, hasStack.line,
          `Request #${i} has the correct line number on top of the JS stack`);
      }
    } else {
      is(stackLen, 0, `Request #${i} (${causeType}) has an empty stacktrace`);
    }
  });

  yield teardown(monitor);
  finish();
});
