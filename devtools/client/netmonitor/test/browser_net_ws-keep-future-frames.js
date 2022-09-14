/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  // Set WS messages limit to a lower value for testing
  await pushPref("devtools.netmonitor.msg.displayed-messages.limit", 10);

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
    await content.wrappedJSObject.openConnection(10);
  });
  await onNetworkEvents;

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 1, "There should be one request");

  // Wait for truncated message notification to appear
  const truncatedMessageWait = waitForDOM(
    document,
    "#messages-view .truncated-message"
  );

  // Select the first request
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);

  // Click on the "Response" panel
  clickOnSidebarTab(document, "response");
  await truncatedMessageWait;

  // Wait for truncated message notification to appear and to have an expected text
  await waitFor(() =>
    document
      .querySelector("#messages-view .truncated-message")
      .textContent.includes("10 messages")
  );

  // Set on 'Keep all future messages' checkbox
  const truncationCheckbox = document.querySelector(
    "#messages-view .truncation-checkbox"
  );
  truncationCheckbox.click();

  // Get rid of all current messages
  const clearButton = document.querySelector(
    "#messages-view .message-list-clear-button"
  );
  clearButton.click();

  // And request new messages
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.sendFrames(10);
  });

  // Wait while they appear in the Response tab
  // We should see all requested messages (sometimes more than 20 = 10 sent + 10 received)
  await waitForDOM(
    document,
    "#messages-view .message-list-table .message-list-item",
    20
  );

  const truncatedMessage = document.querySelector(
    "#messages-view .truncated-message"
  ).textContent;

  is(truncatedMessage, "0 messages have been truncated to conserve memory");

  // Close WS connection
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.closeConnection();
  });

  await teardown(monitor);
});
