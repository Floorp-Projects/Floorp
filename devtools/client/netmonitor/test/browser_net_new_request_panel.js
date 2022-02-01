/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if the New Request panel shows in the left when the pref is true
 */

add_task(async function() {
  // Turn true the pref
  await pushPref("devtools.netmonitor.features.newEditAndResend", true);

  const { monitor } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  info("open the left panel");
  const waitForPanels = waitForDOM(
    document,
    ".monitor-panel .network-action-bar"
  );

  info("switching to new HTTP Custom Request panel");
  let HTTPCustomRequestButton = document.querySelector(
    "#netmonitor-toolbar-container .devtools-http-custom-request-icon"
  );
  ok(HTTPCustomRequestButton, "The Toolbar button should be visible.");

  HTTPCustomRequestButton.click();
  await waitForPanels;

  is(
    !!document.querySelector("#network-action-bar-HTTP-custom-request-panel"),
    true,
    "The 'New Request' header should be visible when the pref is true."
  );

  // Turn false the pref
  await pushPref("devtools.netmonitor.features.newEditAndResend", false);

  // Close the panel to updated after changing the pref
  const closePanel = document.querySelector(
    ".network-action-bar .tabs-navigation .sidebar-toggle"
  );
  closePanel.click();

  // Check if the toolbar is hidden when tre pref is false
  HTTPCustomRequestButton = document.querySelector(
    "#netmonitor-toolbar-container .devtools-http-custom-request-icon"
  );

  is(
    !!HTTPCustomRequestButton,
    false,
    "The toolbar button should be hidden when the pref is false."
  );

  await teardown(monitor);
});

/**
 * Test if the context menu open the new HTTP Custom Request Panel
 * when the pref is true
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 1);

  await pushPref("devtools.netmonitor.features.newEditAndResend", true);

  info("selecting first request");
  const firstRequestItem = document.querySelectorAll(".request-list-item")[0];
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequestItem);
  EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequestItem);

  // Cheking if the item "Resend" is hidden
  is(
    !!getContextMenuItem(monitor, "request-list-context-resend-only"),
    false,
    "The'Resend' item should be hidden when the pref is true."
  );

  info("opening the new request panel");
  const waitForPanels = waitForDOM(
    document,
    ".monitor-panel .network-action-bar"
  );
  getContextMenuItem(monitor, "request-list-context-resend").click();
  await waitForPanels;

  is(
    !!document.querySelector("#network-action-bar-HTTP-custom-request-panel"),
    true,
    "The 'New Request' header should be visible when the pref is true."
  );

  await teardown(monitor);
});
