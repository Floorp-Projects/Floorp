/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if request JS stack is property reported if the request is internally
 * redirected without hitting the network (HSTS is one of such cases)
 */

var test = Task.async(function* () {
  const EXPECTED_REQUESTS = [
    // Request to HTTP URL, redirects to HTTPS, has callstack
    { status: 302, hasStack: true },
    // Serves HTTPS, sets the Strict-Transport-Security header, no stack
    { status: 200, hasStack: false },
    // Second request to HTTP redirects to HTTPS internally
    { status: 200, hasStack: true },
  ];

  let [, debuggee, monitor] = yield initNetMonitor(CUSTOM_GET_URL);
  let { RequestsMenu } = monitor.panelWin.NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  debuggee.performRequests(2, HSTS_SJS);
  yield waitForNetworkEvents(monitor, EXPECTED_REQUESTS.length);

  EXPECTED_REQUESTS.forEach(({status, hasStack}, i) => {
    let { attachment } = RequestsMenu.getItemAtIndex(i);

    is(attachment.status, status, `Request #${i} has the expected status`);

    let { stacktrace } = attachment.cause;
    let stackLen = stacktrace ? stacktrace.length : 0;

    if (hasStack) {
      ok(stacktrace, `Request #${i} has a stacktrace`);
      ok(stackLen > 0, `Request #${i} has a stacktrace with ${stackLen} items`);
    } else {
      is(stackLen, 0, `Request #${i} has an empty stacktrace`);
    }
  });

  // Send a request to reset the HSTS policy to state before the test
  debuggee.performRequests(1, HSTS_SJS + "?reset");
  yield waitForNetworkEvents(monitor, 1);

  yield teardown(monitor);
  finish();
});
