/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Request-Headers and Response-Headers are sorted in Headers tab.
 * The test also verifies that headers with the same name and headers
 * with an empty value are also displayed.
 */
add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_SJS);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  yield wait;

  wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  yield wait;

  yield waitUntil(() => {
    let request = getSortedRequests(store.getState()).get(0);
    return request.requestHeaders && request.responseHeaders;
  });

  info("Check if Request-Headers and Response-Headers are sorted");
  let expectedResponseHeaders = ["cache-control", "connection", "content-length",
                                 "content-type", "date", "expires", "foo-bar",
                                 "foo-bar", "foo-bar", "pragma", "server", "set-cookie",
                                 "set-cookie"];
  let expectedRequestHeaders = ["Accept", "Accept-Encoding", "Accept-Language",
                                "Cache-Control", "Connection", "Cookie", "Host",
                                "Pragma", "Upgrade-Insecure-Requests", "User-Agent"];

  let labelCells = document.querySelectorAll(".treeLabelCell");
  let actualResponseHeaders = [];
  let actualRequestHeaders = [];

  let responseHeadersLength = expectedResponseHeaders.length;
  for (let i = 1; i < responseHeadersLength + 1; i++) {
    actualResponseHeaders.push(labelCells[i].innerText);
  }

  for (let i = responseHeadersLength + 2; i < labelCells.length; i++) {
    actualRequestHeaders.push(labelCells[i].innerText);
  }

  is(actualResponseHeaders.toString(), expectedResponseHeaders.toString(),
    "Response Headers are sorted");

  is(actualRequestHeaders.toString(), expectedRequestHeaders.toString(),
    "Request Headers are sorted");

  yield teardown(monitor);
});
