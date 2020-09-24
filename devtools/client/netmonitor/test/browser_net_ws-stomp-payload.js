/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that WS connection is established successfully and the truncated payload is correct.
 */

add_task(async function() {
  await pushPref("devtools.netmonitor.features.webSockets", true);

  const { tab, monitor } = await initNetMonitor(WS_PAGE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Wait for WS connections to be established + send messages
  const onNetworkEvents = waitForNetworkEvents(monitor, 1);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.openConnection(0);
    content.wrappedJSObject.sendData(
      `SEND\nx-firefox-test:true\ncontent-length:17\n\n[{"key":"value"}]\u0000\n`
    );
  });
  await onNetworkEvents;

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 1, "There should be one request");

  // Wait for all sent/received messages to be displayed in DevTools
  const wait = waitForDOM(
    document,
    "#messages-view .message-list-table .message-list-item",
    2
  );

  // Select the first request
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);

  // Click on the "Response" panel
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#response-tab")
  );
  await wait;

  // Get all messages present in the "Response" panel
  const frames = document.querySelectorAll(
    "#messages-view .message-list-table .message-list-item"
  );

  // Check expected results
  is(frames.length, 2, "There should be two frames");

  // Wait for next tick to do async stuff (The MessagePayload component uses the async function getMessagePayload)
  await waitForTick();
  const waitForData = waitForDOM(document, "#message-formattedData");
  EventUtils.sendMouseEvent({ type: "mousedown" }, frames[0]);

  await waitForData;

  is(
    document.querySelector("#message-formattedData-header").innerText,
    "STOMP",
    "The STOMP payload panel should be displayed"
  );

  ok(
    document.querySelector("#message-formattedData .treeTable"),
    "A tree table should be used to display the formatted payload"
  );

  ok(
    document.getElementById("/command"),
    "The message 'command' should be displayed"
  );
  ok(
    document.getElementById("/headers"),
    "The message 'headers' should be displayed"
  );
  ok(
    document.getElementById("/body"),
    "The message 'body' should be displayed"
  );

  // Close WS connection
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.closeConnection();
  });

  await teardown(monitor);
});
