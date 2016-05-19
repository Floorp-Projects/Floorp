/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if requests intercepted by service workers have the correct status code
 */

// Service workers only work on https
const URL = EXAMPLE_URL.replace("http:", "https:");

const TEST_URL = URL + "service-workers/status-codes.html";

var test = Task.async(function* () {
  let [tab, debuggee, monitor] = yield initNetMonitor(TEST_URL, null, true);
  info("Starting test... ");

  let { document, L10N, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;

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
      }
    },
  ];

  info("Registering the service worker...");
  yield debuggee.registerServiceWorker();

  info("Performing requests...");
  debuggee.performRequests();
  yield waitForNetworkEvents(monitor, REQUEST_DATA.length);

  let index = 0;
  for (let request of REQUEST_DATA) {
    let item = RequestsMenu.getItemAtIndex(index);

    info("Verifying request #" + index);
    yield verifyRequestItemTarget(item, request.method, request.uri, request.details);

    index++;
  }

  yield teardown(monitor);
  finish();
});
