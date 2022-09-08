/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const asyncStorage = require("devtools/shared/async-storage");

/**
 * Test cleaning a custom request.
 */
add_task(async function() {
  // Turn on the pref
  await pushPref("devtools.netmonitor.features.newEditAndResend", true);
  // Reset the storage for the persisted custom request
  await asyncStorage.removeItem("devtools.netmonitor.customRequest");

  const { monitor, tab } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const { getSelectedRequest } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  await performRequests(monitor, tab, 1);

  info("selecting first request");
  const firstRequestItem = document.querySelectorAll(".request-list-item")[0];
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequestItem);
  EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequestItem);

  info("Opening the new request panel");
  const waitForPanels = waitUntil(
    () =>
      document.querySelector(".http-custom-request-panel") &&
      document.querySelector("#http-custom-request-send-button").disabled ===
        false
  );

  await selectContextMenuItem(monitor, "request-list-context-edit-resend");
  await waitForPanels;

  const request = getSelectedRequest(store.getState());

  // Check if the panel is updated with the content by the request clicked
  const urlValue = document.querySelector(".http-custom-url-value");
  is(
    urlValue.textContent,
    request.url,
    "The URL in the form should match the request we clicked"
  );

  info("Clicking on the clear button");
  document.querySelector("#http-custom-request-clear-button").click();
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

  await teardown(monitor);
});
