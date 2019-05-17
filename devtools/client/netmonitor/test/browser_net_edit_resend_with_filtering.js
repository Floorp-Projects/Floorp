/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if resending a XHR request while filtering XHR displays
 * the correct requests
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(POST_RAW_URL);
  const { document, store, windowRequire } = monitor.panelWin;
  const {
    getSelectedRequest,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Execute XHR request and filter by XHR
  await performRequests(monitor, tab, 1);
  document.querySelector(".requests-list-filter-xhr-button").click();

  // Confirm XHR request and click it
  const xhrRequestItem = document.querySelectorAll(".request-list-item")[0];
  EventUtils.sendMouseEvent({ type: "mousedown" }, xhrRequestItem);
  const waitForHeaders = waitUntil(() => document.querySelector(".headers-overview"));
  await waitForHeaders;
  const firstRequest = getSelectedRequest(store.getState());

  // Open context menu and execute "Edit & Resend".
  EventUtils.sendMouseEvent({ type: "contextmenu" }, xhrRequestItem);
  getContextMenuItem(monitor, "request-list-context-resend").click();

  // Wait for "Edit & Resend" panel to appear
  await waitUntil(() => document.querySelector("#custom-request-send-button"));

  // Select the temporary clone-request and check its ID
  // it should be calculated from the original request
  // by appending '-clone' suffix.
  document.querySelectorAll(".request-list-item")[1].click();
  const cloneRequest = getSelectedRequest(store.getState());

  ok(cloneRequest.id.replace(/-clone$/, "") == firstRequest.id,
    "The second XHR request is a clone of the first");

  // Click the "Send" button and wait till the new request appears in the list
  document.querySelector("#custom-request-send-button").click();
  await waitForNetworkEvents(monitor, 1);

  // Filtering by "other" so the resent request is visible after completion
  document.querySelector(".requests-list-filter-other-button").click();

  // Select the new (cloned) request
  document.querySelectorAll(".request-list-item")[0].click();
  const resendRequest = getSelectedRequest(store.getState());

  ok(resendRequest.id !== firstRequest.id,
    "The second XHR request was made and is unique");

  return teardown(monitor);
});
