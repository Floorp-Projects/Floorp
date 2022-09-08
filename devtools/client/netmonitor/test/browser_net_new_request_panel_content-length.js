/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that the content length field is always updated when
 * the message body changes.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(POST_RAW_URL, {
    requestCount: 1,
  });

  info("Starting test... ");

  const { window, document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const { getSelectedRequest } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  await performRequests(monitor, tab, 1);

  info("Select the first request");
  const firstRequest = document.querySelectorAll(".request-list-item")[0];

  const waitForHeaders = waitUntil(() =>
    document.querySelector(".headers-overview")
  );
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);
  await waitForHeaders;

  info("Open the first request in the request panel to resend");
  const waitForPanels = waitUntil(
    () =>
      document.querySelector(".http-custom-request-panel") &&
      document.querySelector("#http-custom-request-send-button").disabled ===
        false
  );

  // Open the context menu and select resend menu item
  EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequest);
  await selectContextMenuItem(monitor, "request-list-context-edit-resend");

  await waitForPanels;

  const contentLengthField = document.querySelector(
    "#http-custom-content-length .http-custom-input-value"
  );

  is(contentLengthField.value, "15", "The content length is correct");

  const messageBody = document.querySelector("#http-custom-postdata-value");
  messageBody.focus();
  EventUtils.sendString("bla=333&", window);

  await waitUntil(() => {
    return contentLengthField.value == "23";
  });
  ok(true, "The content length is  still correct");

  info("Send the request");
  const waitUntilEventsDisplayed = waitForNetworkEvents(monitor, 1);
  document.querySelector("#http-custom-request-send-button").click();
  await waitUntilEventsDisplayed;

  const newRequest = getSelectedRequest(store.getState());

  // Wait for request headers to be available
  await waitUntil(() => newRequest.requestHeaders?.headers.length);

  const contentLengthHeader = newRequest.requestHeaders.headers.find(
    header => header.name == "Content-Length"
  );

  is(
    contentLengthHeader.value,
    "23",
    "The content length of the resent request is correct"
  );

  await teardown(monitor);
});
