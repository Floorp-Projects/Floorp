/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if request JS stack is property reported if the request is internally
 * redirected without hitting the network (HSTS is one of such cases)
 */

add_task(async function() {
  const EXPECTED_REQUESTS = [
    // Request to HTTP URL, redirects to HTTPS, has callstack
    { status: 302, hasStack: true },
    // Serves HTTPS, sets the Strict-Transport-Security header, no stack
    { status: 200, hasStack: false },
    // Second request to HTTP redirects to HTTPS internally
    { status: 200, hasStack: true },
  ];

  const { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL);
  const { store, windowRequire, connector } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, EXPECTED_REQUESTS.length);
  await performRequests(2, HSTS_SJS);
  await wait;

  // Fetch stack-trace data from the backend and wait till
  // all packets are received.
  const requests = getSortedRequests(store.getState())
    .filter((req) => !req.stacktrace)
    .map((req) => connector.requestData(req.id, "stackTrace"));

  await Promise.all(requests);

  EXPECTED_REQUESTS.forEach(({status, hasStack}, i) => {
    const item = getSortedRequests(store.getState()).get(i);

    is(item.status, status, `Request #${i} has the expected status`);

    const { stacktrace } = item;
    const stackLen = stacktrace ? stacktrace.length : 0;

    if (hasStack) {
      ok(stacktrace, `Request #${i} has a stacktrace`);
      ok(stackLen > 0, `Request #${i} has a stacktrace with ${stackLen} items`);
    } else {
      is(stackLen, 0, `Request #${i} has an empty stacktrace`);
    }
  });

  // Send a request to reset the HSTS policy to state before the test
  wait = waitForNetworkEvents(monitor, 1);
  await performRequests(1, HSTS_SJS + "?reset");
  await wait;

  await teardown(monitor);

  function performRequests(count, url) {
    return ContentTask.spawn(tab.linkedBrowser, { count, url }, async function(args) {
      content.wrappedJSObject.performRequests(args.count, args.url);
    });
  }
});
