/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that WS connection is established successfully and the truncated payload is correct.
 */

add_task(async function() {
  await pushPref("devtools.netmonitor.features.webSockets", true);

  // Set WS frame payload limit to a lower value for testing
  await pushPref("devtools.netmonitor.ws.messageDataLimit", 100);

  const { tab, monitor } = await initNetMonitor(WS_PAGE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Wait for WS connections to be established + send messages
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.openConnection(0);
    content.wrappedJSObject.sendData(new Array(10 * 11).toString()); // > 100B payload
  });

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 1, "There should be one request");

  // Wait for all sent/received messages to be displayed in DevTools
  wait = waitForDOM(
    document,
    "#messages-panel .ws-frames-list-table .ws-frame-list-item",
    2
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
  is(frames.length, 2, "There should be two frames");

  // Wait for next tick to do async stuff (The FramePayload component uses the async function getFramePayload)
  await waitForTick();
  EventUtils.sendMouseEvent({ type: "mousedown" }, frames[0]);

  await waitForDOM(document, "#messages-panel .truncated-data-message");

  ok(
    document.querySelector("#messages-panel .truncated-data-message"),
    "Truncated data header shown"
  );
  is(
    document.querySelector("#messages-panel .ws-frame-rawData-payload")
      .textContent.length,
    100,
    "Payload size is kept to the limit"
  );

  // Close WS connection
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.closeConnection();
  });

  await teardown(monitor);
});
