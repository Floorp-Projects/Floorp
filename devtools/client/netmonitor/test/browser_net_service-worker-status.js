/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if requests intercepted by service workers have the correct status code
 */

// Service workers only work on https
const URL = EXAMPLE_URL.replace("http:", "https:");

const TEST_URL = URL + "service-workers/status-codes.html";

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(TEST_URL, null, true);
  info("Starting test... ");

  let { NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

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
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    yield content.wrappedJSObject.registerServiceWorker();
  });

  info("Performing requests...");
  let wait = waitForNetworkEvents(monitor, REQUEST_DATA.length);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  let index = 0;
  for (let request of REQUEST_DATA) {
    let item = RequestsMenu.getItemAtIndex(index);

    info(`Verifying request #${index}`);
    yield verifyRequestItemTarget(RequestsMenu, item,
      request.method, request.uri, request.details);

    let { stacktrace } = item.cause;
    let stackLen = stacktrace ? stacktrace.length : 0;

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
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    yield content.wrappedJSObject.unregisterServiceWorker();
  });

  yield teardown(monitor);
});
