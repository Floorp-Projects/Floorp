/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Request-Cookies and Response-Cookies are sorted in Cookies tab.
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_UNSORTED_COOKIES_SJS);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  tab.linkedBrowser.reload();

  let wait = waitForNetworkEvents(monitor, 1);
  await wait;

  wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  await wait;

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#cookies-tab"));

  info("Check if Request-Cookies and Response-Cookies are sorted");
  const expectedLabelValues = ["Response cookies", "bob", "httpOnly", "value",
                               "foo", "httpOnly", "value", "tom", "httpOnly", "value",
                               "Request cookies", "bob", "foo", "tom"];
  const labelCells = document.querySelectorAll(".treeLabelCell");
  labelCells.forEach(function(val, index) {
    is(val.innerText, expectedLabelValues[index],
    "Actual label value " + val.innerText + " not equal to expected label value "
    + expectedLabelValues[index]);
  });
  await teardown(monitor);
});
