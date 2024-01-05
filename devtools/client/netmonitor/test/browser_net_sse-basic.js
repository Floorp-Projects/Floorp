/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test basic SSE connection.
 */

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(
    "http://mochi.test:8888/browser/devtools/client/netmonitor/test/html_sse-test-page.html",
    {
      requestCount: 1,
    }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  const onNetworkEvents = waitForNetworkEvents(monitor, 1);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.openConnection();
  });
  await onNetworkEvents;

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 1, "There should be one request");

  // Select the request to open the side panel.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);

  // Wait for messages to be displayed in DevTools
  const wait = waitForDOM(
    document,
    "#messages-view .message-list-table .message-list-item",
    1
  );

  // Test that 'Save Response As' is not in the context menu
  EventUtils.sendMouseEvent({ type: "contextmenu" }, requests[0]);

  ok(
    !getContextMenuItem(monitor, "request-list-context-save-response-as"),
    "The 'Save Response As' context menu item should be hidden"
  );

  // Close context menu.
  const contextMenu = monitor.toolbox.topDoc.querySelector(
    'popupset menupopup[menu-api="true"]'
  );
  const popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;

  // Click on the "Response" panel
  clickOnSidebarTab(document, "response");
  await wait;

  // Get all messages present in the "Response" panel
  const frames = document.querySelectorAll(
    "#messages-view .message-list-table .message-list-item"
  );

  // Check expected results
  is(frames.length, 1, "There should be one message");

  is(
    frames[0].querySelector(".message-list-payload").textContent,
    // Initial whitespace comes from ColumnData.
    " Why so serious?",
    "Data column shows correct payload"
  );

  // Closed message may already be here
  if (
    !document.querySelector("#messages-view .msg-connection-closed-message")
  ) {
    await waitForDOM(
      document,
      "#messages-view .msg-connection-closed-message",
      1
    );
  }

  is(
    !!document.querySelector("#messages-view .msg-connection-closed-message"),
    true,
    "Connection closed message should be displayed"
  );

  is(
    document.querySelector(".message-network-summary-count").textContent,
    "1 message",
    "Correct message count is displayed"
  );

  is(
    document.querySelector(".message-network-summary-total-size").textContent,
    "15 B total",
    "Correct total size should be displayed"
  );

  is(
    !!document.querySelector(".message-network-summary-total-millis"),
    true,
    "Total time is displayed"
  );

  is(
    document.getElementById("frame-filter-menu"),
    null,
    "Toolbar filter menu is hidden"
  );

  await waitForTick();

  EventUtils.sendMouseEvent(
    { type: "contextmenu" },
    document.querySelector(".message-list-headers")
  );

  const columns = ["data", "time", "retry", "size", "eventName", "lastEventId"];
  for (const column of columns) {
    is(
      !!getContextMenuItem(monitor, `message-list-header-${column}-toggle`),
      true,
      `Context menu item "${column}" is displayed`
    );
  }
  return teardown(monitor);
});
