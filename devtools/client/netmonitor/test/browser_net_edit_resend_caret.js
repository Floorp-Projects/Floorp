/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if position of caret does not change (resets to the end) after setting
 * header's value to empty string. Also make sure the "method" field stays empty
 * after removing it and resets to its original value when it looses focus.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Reload to have one request in the list.
  const waitForEvents = waitForNetworkEvents(monitor, 1);
  BrowserTestUtils.loadURI(tab.linkedBrowser, SIMPLE_URL);
  await waitForEvents;

  // Open context menu and execute "Edit & Resend".
  const firstRequest = document.querySelectorAll(".request-list-item")[0];
  const waitForHeaders = waitUntil(() => document.querySelector(".headers-overview"));
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);
  await waitForHeaders;
  await waitForRequestData(store, ["requestHeaders"]);
  EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequest);

  // Open "New Request" form
  const contextResend = getContextMenuItem(monitor, "request-list-context-resend");
  contextResend.click();
  await waitUntil(() => document.querySelector("#custom-headers-value"));
  const headersTextarea = document.querySelector("#custom-headers-value");
  await waitUntil(() => document.querySelector("#custom-method-value"));
  const methodField = document.querySelector("#custom-method-value");
  const originalMethodValue = methodField.value;
  const {
    getSelectedRequest,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");
  const request = getSelectedRequest(store.getState());
  const hostHeader = request.requestHeaders.headers[0];

  // Close the open context menu, otherwise sendString will not work
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  headersTextarea.focus();

  // Clean value of host header
  const headersContent = headersTextarea.value;
  const start = "Host: ".length;
  const end = headersContent.indexOf("\n");
  headersTextarea.setSelectionRange(start, end);
  EventUtils.synthesizeKey("VK_DELETE", {});

  methodField.focus();
  methodField.select();
  EventUtils.synthesizeKey("VK_DELETE", {});

  ok(getSelectedRequest(store.getState()).requestHeaders.headers[0] !== hostHeader,
    "Value of Host header was edited and should change"
  );

  ok(headersTextarea.selectionStart === start && headersTextarea.selectionEnd === start,
    "Position of caret should not change"
  );

  ok(getSelectedRequest(store.getState()).method === "",
    "Value of method header was deleted and should be empty"
  );

  headersTextarea.focus();

  ok(getSelectedRequest(store.getState()).method === originalMethodValue,
    "Value of method header should reset to its original value"
  );

  return teardown(monitor);
});
