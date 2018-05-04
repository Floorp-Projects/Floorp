/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Request-Headers and Response-Headers are correctly filtered in Headers tab.
 */
add_task(async function() {
  let { tab, monitor } = await initNetMonitor(SIMPLE_SJS);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await wait;

  wait = waitUntil(() => document.querySelector(".headers-overview"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  await wait;

  await waitForRequestData(store, ["requestHeaders", "responseHeaders"]);

  document.querySelectorAll(".devtools-filterinput")[1].focus();
  EventUtils.synthesizeKey("con", {});
  await waitUntil(() => document.querySelector(".treeRow.hidden"));

  info("Check if Headers are filtered correctly");

  let totalResponseHeaders = ["cache-control", "connection", "content-length",
                              "content-type", "date", "expires", "foo-bar",
                              "foo-bar", "foo-bar", "pragma", "server", "set-cookie",
                              "set-cookie"];
  let expectedResponseHeaders = ["cache-control", "connection", "content-length",
                                 "content-type"];
  let expectedRequestHeaders = ["Cache-Control", "Connection"];

  let labelCells = document.querySelectorAll(".treeLabelCell");
  let filteredResponseHeaders = [];
  let filteredRequestHeaders = [];

  let responseHeadersLength = totalResponseHeaders.length;
  for (let i = 1; i < responseHeadersLength + 1; i++) {
    if (labelCells[i].offsetHeight > 0) {
      filteredResponseHeaders.push(labelCells[i].innerText);
    }
  }

  for (let i = responseHeadersLength + 2; i < labelCells.length; i++) {
    if (labelCells[i].offsetHeight > 0) {
      filteredRequestHeaders.push(labelCells[i].innerText);
    }
  }

  is(filteredResponseHeaders.toString(), expectedResponseHeaders.toString(),
    "Response Headers are filtered");
  is(filteredRequestHeaders.toString(), expectedRequestHeaders.toString(),
    "Request Headers are filtered");

  await teardown(monitor);
});
