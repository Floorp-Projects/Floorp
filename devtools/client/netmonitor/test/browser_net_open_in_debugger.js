/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the 'Open in debugger' feature
 */

add_task(async function() {
  const { tab, monitor, toolbox} = await initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  // Avoid async processing
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[2]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[2]);
  await waitUntil(() =>
    getContextMenuItem(monitor, "request-list-context-open-in-debugger"));

  const onDebuggerReady = toolbox.once("jsdebugger-ready");
  getContextMenuItem(monitor, "request-list-context-open-in-debugger").click();
  await onDebuggerReady;

  ok(true, "Debugger has been open");

  await teardown(monitor);
});
