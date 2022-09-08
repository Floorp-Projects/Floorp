/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const asyncStorage = require("devtools/shared/async-storage");

/**
 * Test if content is still persisted after the panel is closed
 */

add_task(async function() {
  // Turn true the pref
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

  info("open the left panel");
  let waitForPanels = waitForDOM(
    document,
    ".monitor-panel .network-action-bar"
  );

  let HTTPCustomRequestButton = document.querySelector(
    "#netmonitor-toolbar-container .devtools-http-custom-request-icon"
  );
  HTTPCustomRequestButton.click();
  await waitForPanels;

  info("Check if the panel is empty");
  is(
    document.querySelector(".http-custom-url-value").value,
    "",
    "The URL input should be empty"
  );

  info("Add some content on the panel");
  const methodValue = document.querySelector("#http-custom-method-value");
  methodValue.value = "POST";
  methodValue.dispatchEvent(new Event("change", { bubbles: true }));

  const url = document.querySelector(".http-custom-url-value");
  url.focus();
  EventUtils.sendString("https://www.example.com");

  info("Adding new parameters");
  const newParameterName = document.querySelector(
    "#http-custom-query .map-add-new-inputs .http-custom-input-name"
  );
  newParameterName.focus();
  EventUtils.sendString("My-param");

  info("Adding new headers");
  const newHeaderName = document.querySelector(
    "#http-custom-headers .map-add-new-inputs .http-custom-input-name"
  );
  newHeaderName.focus();
  EventUtils.sendString("My-header");

  const newHeaderValue = Array.from(
    document.querySelectorAll(
      "#http-custom-headers .http-custom-input .http-custom-input-value"
    )
  ).pop();
  newHeaderValue.focus();
  EventUtils.sendString("my-value");

  const postValue = document.querySelector("#http-custom-postdata-value");
  postValue.focus();
  EventUtils.sendString("{'Name': 'Value'}");

  // Close the panel
  const closePanel = document.querySelector(
    ".network-action-bar .tabs-navigation .sidebar-toggle"
  );
  closePanel.click();

  // Open the panel again to see if the content is still there
  waitForPanels = waitUntil(
    () =>
      document.querySelector(".http-custom-request-panel") &&
      document.querySelector("#http-custom-request-send-button").disabled ===
        false
  );

  HTTPCustomRequestButton = document.querySelector(
    "#netmonitor-toolbar-container .devtools-http-custom-request-icon"
  );
  HTTPCustomRequestButton.click();
  await waitForPanels;

  is(
    methodValue.value,
    "POST",
    "The content should still be there after the user close the panel and re-opened"
  );

  is(
    url.value,
    "https://www.example.com?My-param=",
    "The url should still be there after the user close the panel and re-opened"
  );

  const [nameParam] = Array.from(
    document.querySelectorAll(
      "#http-custom-query .tabpanel-summary-container.http-custom-input textarea"
    )
  );
  is(
    nameParam.value,
    "My-param",
    "The Parameter name should still be there after the user close the panel and re-opened"
  );

  const [name, value] = Array.from(
    document.querySelectorAll(
      "#http-custom-headers .tabpanel-summary-container.http-custom-input textarea"
    )
  );
  is(
    name.value,
    "My-header",
    "The header name should still be there after the user close the panel and re-opened"
  );
  is(
    value.value,
    "my-value",
    "The header value should still be there after the user close the panel and re-opened"
  );

  is(
    postValue.value,
    "{'Name': 'Value'}",
    "The content should still be there after the user close the panel and re-opened"
  );

  await teardown(monitor);
});
