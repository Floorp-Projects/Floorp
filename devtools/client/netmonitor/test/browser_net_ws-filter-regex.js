/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that RegEx filter is worrking.
 */

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.netmonitor.features.webSockets", true]],
  });
  const { tab, monitor } = await initNetMonitor(WS_PAGE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedFrames } = windowRequire(
    "devtools/client/netmonitor/src/selectors/web-sockets"
  );

  store.dispatch(Actions.batchEnable(false));

  // Wait for WS connection(s) to be established + send messages
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.openConnection(1);
  });

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 1, "There should be one request");

  // Wait for all sent/received messages to be displayed in DevTools
  const wait = waitForDOM(
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
  type("/Payload [0-9]+/");

  // Wait till the text filter is applied.
  await waitUntil(() => getDisplayedFrames(store.getState()).length == 2);

  const filteredFrames = document.querySelectorAll(
    "#messages-panel .ws-frames-list-table .ws-frame-list-item"
  );
  is(filteredFrames.length, 2, "There should be two frames");

  // Close WS connection
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.closeConnection();
  });

  await teardown(monitor);
});
