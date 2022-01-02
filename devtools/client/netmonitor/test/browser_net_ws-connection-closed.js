/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that WS connection is established successfully and message is displayed when the connection is closed.
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

  store.dispatch(Actions.batchEnable(false));

  // Wait for WS connection to be established + send messages
  const onNetworkEvents = waitForNetworkEvents(monitor, 1);
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.openConnection(1);
  });
  await onNetworkEvents;

  const requests = document.querySelectorAll(".request-list-item");

  // Select the request to open the side panel.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);

  // Click on the "Response" panel
  clickOnSidebarTab(document, "response");

  const wait = waitForDOM(
    document,
    "#messages-view .msg-connection-closed-message"
  );

  // Close WS connection
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.closeConnection();
  });
  await wait;

  is(
    !!document.querySelector("#messages-view .msg-connection-closed-message"),
    true,
    "Connection closed message should be displayed"
  );

  await teardown(monitor);
});
