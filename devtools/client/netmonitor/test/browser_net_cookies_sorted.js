/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Request-Cookies and Response-Cookies are sorted in Cookies tab.
 */
add_task(async function() {
  const { monitor } = await initNetMonitor(SIMPLE_UNSORTED_COOKIES_SJS, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1);
  await reloadBrowser();
  await wait;

  wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await wait;

  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  clickOnSidebarTab(document, "cookies");

  info("Check if Request-Cookies and Response-Cookies are sorted");
  const expectedLabelValues = [
    "bob",
    "httpOnly",
    "value",
    "foo",
    "httpOnly",
    "value",
    "tom",
    "httpOnly",
    "value",
    "bob",
    "foo",
    "tom",
  ];
  const labelCells = document.querySelectorAll(".treeLabelCell");
  labelCells.forEach(function(val, index) {
    is(
      val.innerText,
      expectedLabelValues[index],
      "Actual label value " +
        val.innerText +
        " not equal to expected label value " +
        expectedLabelValues[index]
    );
  });

  const lastItem = document.querySelector(
    "#cookies-panel .properties-view tr.treeRow:last-child"
  );
  lastItem.scrollIntoView();

  info("Checking for unwanted scrollbars appearing in the tree view");
  const view = document.querySelector(
    "#cookies-panel .properties-view .treeTable"
  );
  is(scrolledToBottom(view), true, "The view is not scrollable");

  await teardown(monitor);

  function scrolledToBottom(element) {
    return element.scrollTop + element.clientHeight >= element.scrollHeight;
  }
});
