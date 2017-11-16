/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Open in new tab works.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  info("Starting test...");

  let { document, store, windowRequire } = monitor.panelWin;
  let contextMenuDoc = monitor.panelWin.parent.document;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(1);
  });
  yield wait;

  wait = waitForDOM(contextMenuDoc, "#request-list-context-newtab");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[0]);
  yield wait;

  let onTabOpen = once(gBrowser.tabContainer, "TabOpen", false);
  monitor.panelWin.parent.document
    .querySelector("#request-list-context-newtab").click();
  yield onTabOpen;

  ok(true, "A new tab has been opened");

  yield teardown(monitor);

  gBrowser.removeCurrentTab();
});
