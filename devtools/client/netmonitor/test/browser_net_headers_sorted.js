/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Request-Headers and Response-Headers are sorted in Headers tab.
 * The test also verifies that headers with the same name and headers
 * with an empty value are also displayed.
 *
 * The test also checks that raw headers are displayed in the original
 * order and not sorted.
 */
add_task(async function() {
  let { tab, monitor } = await initNetMonitor(SIMPLE_SJS);
  info("Starting test... ");

  let { store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await wait;

  // Verify request and response headers.
  await verifyHeaders(monitor);
  await verifyRawHeaders(monitor);

  // Clean up
  await teardown(monitor);
});

async function verifyHeaders(monitor) {
  let { document, store } = monitor.panelWin;

  info("Check if Request-Headers and Response-Headers are sorted");

  let wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  await wait;

  await waitForRequestData(store, ["requestHeaders", "responseHeaders"]);

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
}

async function verifyRawHeaders(monitor) {
  let { document } = monitor.panelWin;

  info("Check if raw Request-Headers and raw Response-Headers are not sorted");

  let actualResponseHeaders = [];
  let actualRequestHeaders = [];

  let expectedResponseHeaders = ["cache-control", "pragma", "expires",
                                 "set-cookie", "set-cookie", "content-type", "foo-bar",
                                 "foo-bar", "foo-bar", "connection", "server",
                                 "date", "content-length"];

  let expectedRequestHeaders = ["Host", "User-Agent", "Accept", "Accept-Language",
                                "Accept-Encoding", "Cookie", "Connection",
                                "Upgrade-Insecure-Requests", "Pragma",
                                "Cache-Control"];

  // Click the 'Raw headers' button to show original headers source.
  let rawHeadersBtn = document.querySelector(".raw-headers-button");
  rawHeadersBtn.click();

  // Wait till raw headers are available.
  await waitUntil(() => {
    return document.querySelector(".raw-request-headers-textarea") &&
      document.querySelector(".raw-response-headers-textarea");
  });

  let requestHeadersText =
    document.querySelector(".raw-request-headers-textarea").textContent;
  let responseHeadersText =
    document.querySelector(".raw-response-headers-textarea").textContent;

  let rawRequestHeadersArray = requestHeadersText.split("\n");
  for (let i = 0; i < rawRequestHeadersArray.length; i++) {
    let header = rawRequestHeadersArray[i];
    actualRequestHeaders.push(header.split(":")[0]);
  }

  let rawResponseHeadersArray = responseHeadersText.split("\n");
  for (let i = 1; i < rawResponseHeadersArray.length; i++) {
    let header = rawResponseHeadersArray[i];
    actualResponseHeaders.push(header.split(":")[0]);
  }

  is(actualResponseHeaders.toString(), expectedResponseHeaders.toString(),
    "Raw Response Headers are not sorted");

  is(actualRequestHeaders.toString(), expectedRequestHeaders.toString(),
    "Raw Request Headers are not sorted");
}
