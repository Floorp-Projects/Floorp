/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const asyncStorage = require("devtools/shared/async-storage");

/**
 * Test if the New Request panel shows up as a expected when opened from the toolbar
 */

add_task(async function() {
  // Turn on the pref
  await pushPref("devtools.netmonitor.features.newEditAndResend", true);
  // Reset the storage for the persisted custom request
  await asyncStorage.removeItem("devtools.netmonitor.customRequest");

  const { monitor } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  info("Open the HTTP Custom Panel through the toolbar button");
  let HTTPCustomRequestButton = document.querySelector(
    "#netmonitor-toolbar-container .devtools-http-custom-request-icon"
  );
  ok(HTTPCustomRequestButton, "The Toolbar button should be visible.");
  const waitForPanels = waitForDOM(
    document,
    ".monitor-panel .network-action-bar"
  );
  HTTPCustomRequestButton.click();
  await waitForPanels;

  is(
    !!document.querySelector("#network-action-bar-HTTP-custom-request-panel"),
    true,
    "The 'New Request' header should be visible when the pref is true."
  );
  is(
    !!document.querySelector(
      ".devtools-button.devtools-http-custom-request-icon.checked"
    ),
    true,
    "The toolbar button should be highlighted"
  );

  info("if the default state is empty");
  is(
    document.querySelector(".http-custom-method-value").value,
    "GET",
    "The method input should be 'GET' by default"
  );
  is(
    document.querySelector(".http-custom-url-value").textContent,
    "",
    "The URL input should be empty"
  );
  const urlParametersValue = document.querySelectorAll(
    "#http-custom-query .tabpanel-summary-container.http-custom-input"
  );
  is(urlParametersValue.length, 0, "The URL Parameters input should be empty");
  const headersValue = document.querySelectorAll(
    "#http-custom-headers .tabpanel-summary-container.http-custom-input"
  );
  is(headersValue.length, 0, "The Headers input should be empty");
  is(
    document.querySelector("#http-custom-postdata-value").textContent,
    "",
    "The Post body input should be empty"
  );

  // Turn off the pref
  await pushPref("devtools.netmonitor.features.newEditAndResend", false);
  info("Close the panel to update the interface after changing the pref");
  const closePanel = document.querySelector(
    ".network-action-bar .tabs-navigation .sidebar-toggle"
  );
  closePanel.click();

  info("Check if the toolbar button is hidden when the pref is false");
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
