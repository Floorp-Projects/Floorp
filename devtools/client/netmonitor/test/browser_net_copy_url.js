/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying a request's url works.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  // Execute requests.
  await performRequests(monitor, tab, 1);

  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );

  const requestItem = getSortedRequests(store.getState())[0];

  info("Simulating CmdOrCtrl+C on a first element of the request table");
  await waitForClipboardPromise(
    () => synthesizeKeyShortcut("CmdOrCtrl+C"),
    requestItem.url
  );

  emptyClipboard();

  info("Simulating context click on a first element of the request table");
  EventUtils.sendMouseEvent(
    { type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[0]
  );

  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "request-list-context-copy-url").click();
  }, requestItem.url);

  await teardown(monitor);
});
