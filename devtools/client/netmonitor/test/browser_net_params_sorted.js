/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether keys in Params panel are sorted.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 2);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  yield wait;

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#params-tab"));

  let actualKeys = document.querySelectorAll(".treeLabel");
  let expectedKeys = ["Query string", "baz", "foo", "type",
                      "Form data", "baz", "foo"];

  for (let i = 0; i < actualKeys.length; i++) {
    is(actualKeys[i].innerText, expectedKeys[i],
      "Actual value " + actualKeys[i].innerText + " is equal to the " +
      "expected value " + expectedKeys[i]);
  }
});
