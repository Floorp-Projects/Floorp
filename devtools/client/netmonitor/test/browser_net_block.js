/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test blocking and unblocking a request.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const { getSelectedRequest } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Reload to have one request in the list
  let waitForEvents = waitForNetworkEvents(monitor, 1);
  BrowserTestUtils.loadURI(tab.linkedBrowser, SIMPLE_URL);
  await waitForEvents;

  // Capture normal request
  let normalRequestState;
  let normalRequestSize;
  {
    const firstRequest = document.querySelectorAll(".request-list-item")[0];
    const waitForHeaders = waitUntil(() =>
      document.querySelector(".headers-overview")
    );
    EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);
    await waitForHeaders;
    normalRequestState = getSelectedRequest(store.getState());
    normalRequestSize = firstRequest.querySelector(".requests-list-transferred")
      .textContent;
    info("Captured normal request");
    // Mark as blocked
    EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequest);
    const contextBlock = getContextMenuItem(
      monitor,
      "request-list-context-block-url"
    );
    contextBlock.click();
    info("Set request to blocked");
  }

  // Reload to have one request in the list
  info("Reloading to check block");
  // We can't use the normal waiting methods because a canceled request won't send all
  // the extra update packets.
  waitForEvents = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await waitForEvents;

  // Capture blocked request, then unblock
  let blockedRequestState;
  let blockedRequestSize;
  {
    const firstRequest = document.querySelectorAll(".request-list-item")[0];
    blockedRequestSize = firstRequest.querySelector(
      ".requests-list-transferred"
    ).textContent;
    EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);
    blockedRequestState = getSelectedRequest(store.getState());
    info("Captured blocked request");
    // Mark as unblocked
    EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequest);
    const contextUnblock = getContextMenuItem(
      monitor,
      "request-list-context-unblock-url"
    );
    contextUnblock.click();
    info("Set request to unblocked");
  }

  // Reload to have one request in the list
  info("Reloading to check unblock");
  waitForEvents = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await waitForEvents;

  // Capture unblocked request
  let unblockedRequestState;
  let unblockedRequestSize;
  {
    const firstRequest = document.querySelectorAll(".request-list-item")[0];
    unblockedRequestSize = firstRequest.querySelector(
      ".requests-list-transferred"
    ).textContent;
    EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);
    unblockedRequestState = getSelectedRequest(store.getState());
    info("Captured unblocked request");
  }

  ok(!normalRequestState.blockedReason, "Normal request is not blocked");
  ok(!normalRequestSize.includes("Blocked"), "Normal request has a size");

  ok(blockedRequestState.blockedReason, "Blocked request is blocked");
  ok(
    blockedRequestSize.includes("Blocked"),
    "Blocked request shows reason as size"
  );

  ok(!unblockedRequestState.blockedReason, "Unblocked request is not blocked");
  ok(!unblockedRequestSize.includes("Blocked"), "Unblocked request has a size");

  return teardown(monitor);
});
