/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the 'Open in debugger' feature
 */

add_task(function* () {
  let { tab, monitor, toolbox} = yield initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let contextMenuDoc = monitor.panelWin.parent.document;
  // Avoid async processing
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  wait = waitForDOM(contextMenuDoc, "#request-list-context-open-in-debugger");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[2]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[2]);
  yield wait;

  let onDebuggerReady = toolbox.once("jsdebugger-ready");
  contextMenuDoc.querySelector("#request-list-context-open-in-debugger").click();
  yield onDebuggerReady;

  ok(true, "Debugger has been open");

  yield teardown(monitor);
});
