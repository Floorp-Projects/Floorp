/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if the 'Same site' cookie attribute is correctly set in the cookie panel
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SET_COOKIE_SAME_SITE_SJS);
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

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#cookies-tab"));

  info("Checking the SameSite property");
  const expectedValues = [
    {
        key: "Response cookies",
        value: ""
    },
    {
        key: "foo",
        value: ""
    },
    {
        key: "samesite",
        value: "Lax"
    },
    {
        key: "value",
        value: "bar"
    },
    {
        key: "Request cookies",
        value: ""
    },
    {
        key: "foo",
        value: "bar"
    },
  ];
  const labelCells = document.querySelectorAll(".treeLabelCell");
  const valueCells = document.querySelectorAll(".treeValueCell");
  is(valueCells.length, labelCells.length, "Number of labels "
        + labelCells.length + " different from number of values " + valueCells.length);

  // Go through the cookie properties and check if each one has the expected
  // label and value
  for (let index = 0; index < labelCells.length; ++index) {
    is(labelCells[index].innerText, expectedValues[index].key,
    "Actual label " + labelCells[index].innerText
      + " not equal to expected label " + expectedValues[index].key);
    is(valueCells[index].innerText, expectedValues[index].value,
    "Actual value " + valueCells[index].innerText
      + " not equal to expected value " + expectedValues[index].value);
  }

  await teardown(monitor);
});
