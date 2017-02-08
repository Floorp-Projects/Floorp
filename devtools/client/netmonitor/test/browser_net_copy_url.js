/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying a request's url works.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  info("Starting test... ");

  let { document, gStore, windowRequire } = monitor.panelWin;
  let { getSortedRequests } = windowRequire("devtools/client/netmonitor/selectors/index");

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(1);
  });
  yield wait;

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[0]);

  let requestItem = getSortedRequests(gStore.getState()).get(0);

  yield waitForClipboardPromise(function setup() {
    // Context menu is appending in XUL document, we must select it from
    // toolbox.doc
    monitor.toolbox.doc
      .querySelector("#request-list-context-copy-url").click();
  }, requestItem.url);

  yield teardown(monitor);
});
