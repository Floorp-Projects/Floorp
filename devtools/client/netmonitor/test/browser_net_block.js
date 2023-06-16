/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test blocking and unblocking a request.
 */

add_task(async function () {
  const { monitor, tab } = await initNetMonitor(HTTPS_SIMPLE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const { getSelectedRequest } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Reload to have one request in the list
  let waitForEvents = waitForNetworkEvents(monitor, 1);
  await navigateTo(HTTPS_SIMPLE_URL);
  await waitForEvents;

  // Capture normal request
  let normalRequestState;
  let normalRequestSize;
  let normalHeadersSectionSize;
  let normalFirstHeaderSectionTitle;
  {
    // Wait for the response and request header sections
    const waitForHeaderSections = waitForDOM(
      document,
      "#headers-panel .accordion-item",
      2
    );

    const firstRequest = document.querySelectorAll(".request-list-item")[0];
    EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);

    await waitForHeaderSections;

    const headerSections = document.querySelectorAll(
      "#headers-panel .accordion-item"
    );
    normalRequestState = getSelectedRequest(store.getState());
    normalRequestSize = firstRequest.querySelector(
      ".requests-list-transferred"
    ).textContent;
    normalHeadersSectionSize = headerSections.length;
    normalFirstHeaderSectionTitle = headerSections[0].querySelector(
      ".accordion-header-label"
    ).textContent;

    info("Captured normal request");

    // Mark as blocked
    EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequest);
    const onRequestBlocked = waitForDispatch(
      store,
      "REQUEST_BLOCKING_UPDATE_COMPLETE"
    );

    await selectContextMenuItem(monitor, "request-list-context-block-url");

    info("Wait for selected request to be blocked");
    await onRequestBlocked;
    info("Selected request is now blocked");
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
  let blockedHeadersSectionSize;
  let blockedFirstHeaderSectionTitle;
  {
    const waitForHeaderSections = waitForDOM(
      document,
      "#headers-panel .accordion-item",
      1
    );

    const firstRequest = document.querySelectorAll(".request-list-item")[0];
    EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);

    await waitForHeaderSections;

    const headerSections = document.querySelectorAll(
      "#headers-panel .accordion-item"
    );
    blockedRequestSize = firstRequest.querySelector(
      ".requests-list-transferred"
    ).textContent;
    blockedRequestState = getSelectedRequest(store.getState());
    blockedHeadersSectionSize = headerSections.length;
    blockedFirstHeaderSectionTitle = headerSections[0].querySelector(
      ".accordion-header-label"
    ).textContent;

    info("Captured blocked request");

    // Mark as unblocked
    EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequest);
    const onRequestUnblocked = waitForDispatch(
      store,
      "REQUEST_BLOCKING_UPDATE_COMPLETE"
    );

    await selectContextMenuItem(monitor, "request-list-context-unblock-url");

    info("Wait for selected request to be unblocked");
    await onRequestUnblocked;
    info("Selected request is now unblocked");
  }

  // Reload to have one request in the list
  info("Reloading to check unblock");
  waitForEvents = waitForNetworkEvents(monitor, 1);
  await reloadBrowser();
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
  ok(normalHeadersSectionSize == 2, "Both header sections are showing");
  ok(
    normalFirstHeaderSectionTitle.includes("Response"),
    "The response header section is visible for normal requests"
  );

  ok(blockedRequestState.blockedReason, "Blocked request is blocked");
  ok(
    blockedRequestSize.includes("Blocked"),
    "Blocked request shows reason as size"
  );
  ok(blockedHeadersSectionSize == 1, "Only one header section is showing");
  ok(
    blockedFirstHeaderSectionTitle.includes("Request"),
    "The response header section is not visible for blocked requests"
  );

  ok(!unblockedRequestState.blockedReason, "Unblocked request is not blocked");
  ok(!unblockedRequestSize.includes("Blocked"), "Unblocked request has a size");

  return teardown(monitor);
});
