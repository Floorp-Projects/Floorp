/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if original request's header panel is visible when custom request is cancelled.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const {
    getSelectedRequest,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Reload to have one request in the list
  const waitForEvents = waitForNetworkEvents(monitor, 1);
  BrowserTestUtils.loadURI(tab.linkedBrowser, SIMPLE_URL);
  await waitForEvents;

  // Context Menu > "Edit & Resend"
  const firstRequest = document.querySelectorAll(".request-list-item")[0];
  const waitForHeaders = waitUntil(() => document.querySelector(".headers-overview"));
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);
  await waitForHeaders;
  EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequest);
  const firstRequestState = getSelectedRequest(store.getState());
  const contextResend = getContextMenuItem(monitor, "request-list-context-resend");
  contextResend.click();

  // Waits for "Edit & Resend" panel to appear > New request "Cancel"
  await waitUntil(() => document.querySelector(".custom-request-panel"));
  document.querySelector("#custom-request-close-button").click();
  const finalRequestState = getSelectedRequest(store.getState());

  ok(firstRequestState.id === finalRequestState.id,
    "Original request is selected after cancel button is clicked"
  );

  ok(document.querySelector(".headers-overview") !== null,
    "Request is selected and headers panel is visible"
  );

  return teardown(monitor);
});
