/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Request-Cookies and Response-Cookies are sorted in Cookies tab.
 */
add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_UNSORTED_COOKIES_SJS);
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

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#cookies-tab"));

  info("Check if Request-Cookies and Response-Cookies are sorted");
  let expectedLabelValues = ["Response cookies", "bob", "httpOnly", "value",
                             "foo", "httpOnly", "value", "tom", "httpOnly", "value",
                             "Request cookies", "bob", "foo", "tom"];
  let labelCells = document.querySelectorAll(".treeLabelCell");
  labelCells.forEach(function (val, index) {
    is(val.innerText, expectedLabelValues[index],
    "Actual label value " + val.innerText + " not equal to expected label value "
    + expectedLabelValues[index]);
  });
  yield teardown(monitor);
});
