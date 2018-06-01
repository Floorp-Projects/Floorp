/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether keys in Params panel are sorted.
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(POST_ARRAY_DATA_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 1);

  wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  await wait;

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#params-tab"));

  // The Params panel should render the following
  // POSTed JSON data structure:
  //
  // ▼ JSON
  //   ▼ watches: […]
  //       0: hello
  //       1: how
  //       2: are
  //       3: you
  //     ▼ 4: {…}
  //         a: 10
  //       ▼ b: […]
  //           0: "a"
  //           1: "c"
  //           2: "b"
  //         c: 15
  const actualKeys = document.querySelectorAll(".treeTable .treeRow");
  const expectedKeys = [
    "JSON",
    "watches: [...]",
    "0: hello",
    "1: how",
    "2: are",
    "3: you",
    "4: {...}",
    "a: 10",
    "b: [...]",
    "0: a",
    "1: c",
    "2: b",
    "c: 15",
  ];

  for (let i = 0; i < actualKeys.length; i++) {
    is(actualKeys[i].innerText, expectedKeys[i],
      "Actual value " + actualKeys[i].innerText + " is equal to the " +
      "expected value " + expectedKeys[i]);
  }
});
