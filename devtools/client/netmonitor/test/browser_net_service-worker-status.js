/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if requests intercepted by service workers have the correct status code
 */

// Service workers only work on https
const URL = EXAMPLE_URL.replace("http:", "https:");

const TEST_URL = URL + "service-workers/status-codes.html";

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(TEST_URL, true);
  info("Starting test... ");

  const { document, store, windowRequire, connector } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  const REQUEST_DATA = [
    {
      method: "GET",
      uri: URL + "service-workers/test/200",
      details: {
        status: 200,
        statusText: "OK (service worker)",
        displayedStatus: "service worker",
        type: "plain",
        fullMimeType: "text/plain; charset=UTF-8"
      },
      stackFunctions: ["doXHR", "performRequests"]
    },
  ];

  info("Registering the service worker...");
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await content.wrappedJSObject.registerServiceWorker();
  });

  info("Performing requests...");
  // Execute requests.
  await performRequests(monitor, tab, REQUEST_DATA.length);

  // Fetch stack-trace data from the backend and wait till
  // all packets are received.
  const requests = getSortedRequests(store.getState());
  await Promise.all(requests.map(requestItem =>
    connector.requestData(requestItem.id, "stackTrace")));

  const requestItems = document.querySelectorAll(".request-list-item");
  for (const requestItem of requestItems) {
    requestItem.scrollIntoView();
    const requestsListStatus = requestItem.querySelector(".status-code");
    EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
    await waitUntil(() => requestsListStatus.title);
  }

  let index = 0;
  for (const request of REQUEST_DATA) {
    const item = getSortedRequests(store.getState()).get(index);

    info(`Verifying request #${index}`);
    await verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      item,
      request.method,
      request.uri,
      request.details
    );

    const { stacktrace } = item;
    const stackLen = stacktrace ? stacktrace.length : 0;

    ok(stacktrace, `Request #${index} has a stacktrace`);
    ok(stackLen >= request.stackFunctions.length,
      `Request #${index} has a stacktrace with enough (${stackLen}) items`);

    request.stackFunctions.forEach((functionName, j) => {
      is(stacktrace[j].functionName, functionName,
      `Request #${index} has the correct function at position #${j} on the stack`);
    });

    index++;
  }

  info("Unregistering the service worker...");
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await content.wrappedJSObject.unregisterServiceWorker();
  });

  await teardown(monitor);
});
