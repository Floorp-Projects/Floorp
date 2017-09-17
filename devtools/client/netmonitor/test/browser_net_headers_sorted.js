/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Request-Headers and Response-Headers are sorted in Headers tab.
 */
add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_SJS);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  tab.linkedBrowser.reload();

  let wait = waitForNetworkEvents(monitor, 1);
  yield wait;

  wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  yield wait;

  info("Check if Request-Headers and Response-Headers are sorted");
  let expectedLabelValues = ["Response headers (330 B)", "cache-control", "connection",
                             "content-length", "content-type", "date", "expires",
                             "foo-bar", "pragma", "server", "set-cookie",
                             "Request headers (466 B)", "Accept", "Accept-Encoding",
                             "Accept-Language", "Cache-Control", "Connection", "Cookie",
                             "Host", "Pragma", "Upgrade-Insecure-Requests", "User-Agent"];
  let labelCells = document.querySelectorAll(".treeLabelCell");
  labelCells.forEach(function (val, index) {
    is(val.innerText, expectedLabelValues[index],
    "Actual label value " + val.innerText + " not equal to expected label value "
    + expectedLabelValues[index]);
  });
  yield teardown(monitor);
});
