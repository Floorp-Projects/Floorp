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

  const { document, store, windowRequire, parent } = monitor.panelWin;
  const parentDocument = parent.document;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Execute XHR request and filter by XHR
  await performRequests(monitor, tab, 1);
  document.querySelector(".requests-list-filter-xhr-button").click();

  // Confirm XHR request and click it
  const xhrRequestItem = document.querySelectorAll(".request-list-item")[0];
  EventUtils.sendMouseEvent({ type: "mousedown" }, xhrRequestItem);

  const {
    getSelectedRequest,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");
  const firstRequest = getSelectedRequest(store.getState());

  // Open context menu and execute "Edit & Resend".
  EventUtils.sendMouseEvent({ type: "contextmenu" }, xhrRequestItem);
  parentDocument.querySelector("#request-list-context-resend").click();

  // Click Resend
  await waitUntil(() => document.querySelector("#custom-request-send-button"));
  document.querySelector("#custom-request-send-button").click();

  // Filtering by "other" so the resent request is visible after completion
  document.querySelector(".requests-list-filter-other-button").click();

  // Select the cloned request
  document.querySelectorAll(".request-list-item")[0].click();
  const resendRequest = getSelectedRequest(store.getState());

  ok(resendRequest.id !== firstRequest.id,
    "The second XHR request was made and is unique");

  ok(resendRequest.id.replace(/-clone$/, "") == firstRequest.id,
    "The second XHR request is a clone of the first");

  return teardown(monitor);
});
