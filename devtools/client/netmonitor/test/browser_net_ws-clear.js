/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that WS connection is established successfully and clearing messages works correctly.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(WS_PAGE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Wait for WS connection(s) to be established + send messages
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.openConnection(2);
    await content.wrappedJSObject.openConnection(1);
  });

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 2, "There should be two requests");

  // Wait for all sent/received messages to be displayed in DevTools
  wait = waitForDOM(
    document,
    "#messages-panel .ws-frames-list-table .ws-frame-list-item",
    4
  );

  // Select the first request
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);

  // Click on the "Messages" panel
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#messages-tab")
  );
  await wait;

  // Get all messages present in the "Messages" panel
  const frames = document.querySelectorAll(
    "#messages-panel .ws-frames-list-table .ws-frame-list-item"
  );

  // Check expected results
  is(frames.length, 4, "There should be four frames");

  // Clear messages
  const clearButton = document.querySelector(
    "#messages-panel .ws-frames-list-clear-button"
  );
  clearButton.click();
  is(
    document.querySelectorAll(".ws-frame-list-empty-notice").length,
    1,
    "Empty notice visible"
  );

  // Select the second request and check that the messages are not cleared
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[1]);
  wait = waitForDOM(
    document,
    "#messages-panel .ws-frames-list-table .ws-frame-list-item",
    2
  );
  await wait;
  const secondRequestFrames = document.querySelectorAll(
    "#messages-panel .ws-frames-list-table .ws-frame-list-item"
  );
  is(secondRequestFrames.length, 2, "There should be two frames");

  // Close WS connection
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.closeConnection();
  });

  await teardown(monitor);
});
