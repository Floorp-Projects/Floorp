/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that WS connection is established successfully and sent/received messages are correct.
 */

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.netmonitor.features.webSockets", true]],
  });

  const { tab, monitor } = await initNetMonitor(WS_PAGE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Wait for WS connection to be established + send messages
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.openConnection(1);
  });

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 1, "There should be one request");

  // Select the request to open the side panel.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);

  // Wait for all sent/received messages to be displayed in DevTools
  wait = waitForDOM(
    document,
    "#messages-panel .ws-frames-list-table .ws-frame-list-item",
    2
  );

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

  // Sent frame
  is(
    frames[0].children[0].textContent,
    " Payload 0",
    "The correct sent payload should be displayed"
  );
  is(frames[0].classList.contains("sent"), true, "The payload type is 'Sent'");

  // Received frame
  is(
    frames[1].children[0].textContent,
    " Payload 0",
    "The correct received payload should be displayed"
  );
  is(
    frames[1].classList.contains("received"),
    true,
    "The payload type is 'Received'"
  );

  // Close WS connection
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.closeConnection();
  });

  await teardown(monitor);
});
