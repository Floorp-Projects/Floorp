/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that WS connection is established successfully and filtering messages using freetext works correctly.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(WS_PAGE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedFrames } = windowRequire(
    "devtools/client/netmonitor/src/selectors/web-sockets"
  );

  store.dispatch(Actions.batchEnable(false));

  // Wait for WS connection(s) to be established + send messages
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.openConnection(3);
    await content.wrappedJSObject.openConnection(1);
  });

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 2, "There should be two requests");

  // Wait for all sent/received messages to be displayed in DevTools
  wait = waitForDOM(
    document,
    "#messages-panel .ws-frames-list-table .ws-frame-list-item",
    6
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
  is(frames.length, 6, "There should be six frames");

  // Fill filter input with text and check displayed messages
  const type = string => {
    for (const ch of string) {
      EventUtils.synthesizeKey(ch, {}, monitor.panelWin);
    }
  };
  const filterInput = document.querySelector(
    "#messages-panel .devtools-filterinput"
  );
  filterInput.focus();
  type("Payload 2");

  // Wait till the text filter is applied.
  await waitUntil(() => getDisplayedFrames(store.getState()).length == 2);

  const filteredFrames = document.querySelectorAll(
    "#messages-panel .ws-frames-list-table .ws-frame-list-item"
  );
  is(filteredFrames.length, 2, "There should be two frames");

  // Select the second request and check that the filter input is cleared
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[1]);
  const secondRequestFrames = document.querySelectorAll(
    "#messages-panel .ws-frames-list-table .ws-frame-list-item"
  );
  is(secondRequestFrames.length, 2, "There should be two frames");
  is(filterInput.value, "", "The filter input is cleared");

  // Close WS connection
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.closeConnection();
  });

  await teardown(monitor);
});
