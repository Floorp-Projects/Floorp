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
  const { tab, monitor } = await initNetMonitor(SIMPLE_SJS);
  info("Starting test... ");

  const { store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await wait;

  // Verify request and response headers.
  await verifyHeaders(monitor);
  await verifyRawHeaders(monitor);

  // Clean up
  await teardown(monitor);
});

async function verifyHeaders(monitor) {
  const { document, store } = monitor.panelWin;

  info("Check if Request-Headers and Response-Headers are sorted");

  const wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  await wait;

  await waitForRequestData(store, ["requestHeaders", "responseHeaders"]);

  const expectedResponseHeaders = ["cache-control", "connection", "content-length",
                                   "content-type", "date", "expires", "foo-bar",
                                   "foo-bar", "foo-bar", "pragma", "server", "set-cookie",
                                   "set-cookie"];
  const expectedRequestHeaders = ["Accept", "Accept-Encoding", "Accept-Language",
                                  "Cache-Control", "Connection", "Cookie", "Host",
                                  "Pragma", "Upgrade-Insecure-Requests", "User-Agent"];

  const labelCells = document.querySelectorAll(".treeLabelCell");
  const actualResponseHeaders = [];
  const actualRequestHeaders = [];

  const responseHeadersLength = expectedResponseHeaders.length;
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
  const { document } = monitor.panelWin;

  info("Check if raw Request-Headers and raw Response-Headers are not sorted");

  const actualResponseHeaders = [];
  const actualRequestHeaders = [];

  const expectedResponseHeaders = ["cache-control", "pragma", "expires",
                                   "set-cookie", "set-cookie", "content-type", "foo-bar",
                                   "foo-bar", "foo-bar", "connection", "server",
                                   "date", "content-length"];

  const expectedRequestHeaders = ["Host", "User-Agent", "Accept", "Accept-Language",
                                  "Accept-Encoding", "Cookie", "Connection",
                                  "Upgrade-Insecure-Requests", "Pragma",
                                  "Cache-Control"];

  // Click the 'Raw headers' button to show original headers source.
  const rawHeadersBtn = document.querySelector(".raw-headers-button");
  rawHeadersBtn.click();

  // Wait till raw headers are available.
  await waitUntil(() => {
    return document.querySelector(".raw-request-headers-textarea") &&
      document.querySelector(".raw-response-headers-textarea");
  });

  const requestHeadersText =
    document.querySelector(".raw-request-headers-textarea").textContent;
  const responseHeadersText =
    document.querySelector(".raw-response-headers-textarea").textContent;

  const rawRequestHeadersArray = requestHeadersText.split("\n");
  for (let i = 0; i < rawRequestHeadersArray.length; i++) {
    const header = rawRequestHeadersArray[i];
    actualRequestHeaders.push(header.split(":")[0]);
  }

  const rawResponseHeadersArray = responseHeadersText.split("\n");
  for (let i = 1; i < rawResponseHeadersArray.length; i++) {
    const header = rawResponseHeadersArray[i];
    actualResponseHeaders.push(header.split(":")[0]);
  }

  is(actualResponseHeaders.toString(), expectedResponseHeaders.toString(),
    "Raw Response Headers are not sorted");

  is(actualRequestHeaders.toString(), expectedRequestHeaders.toString(),
    "Raw Request Headers are not sorted");
}
