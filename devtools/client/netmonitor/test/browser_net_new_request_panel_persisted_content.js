/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const asyncStorage = require("resource://devtools/shared/async-storage.js");

/**
 * Test if content is still persisted after the panel is closed
 */

async function addCustomRequestTestContent(tab, monitor, document) {
  info("Open the left panel");
  const waitForPanels = waitForDOM(
    document,
    ".monitor-panel .network-action-bar"
  );

  const HTTPCustomRequestButton = document.querySelector(
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
  EventUtils.sendString("My-param", monitor.panelWin);

  info("Adding new headers");
  const newHeaderName = document.querySelector(
    "#http-custom-headers .map-add-new-inputs .http-custom-input-name"
  );
  newHeaderName.focus();
  EventUtils.sendString("My-header", monitor.panelWin);

  const newHeaderValue = Array.from(
    document.querySelectorAll(
      "#http-custom-headers .http-custom-input .http-custom-input-value"
    )
  ).at(-1);
  newHeaderValue.focus();
  EventUtils.sendString("my-value", monitor.panelWin);

  const postValue = document.querySelector("#http-custom-postdata-value");
  postValue.focus();
  EventUtils.sendString("{'Name': 'Value'}", monitor.panelWin);

  info("Close the panel");
  const closePanel = document.querySelector(
    ".network-action-bar .tabs-navigation .sidebar-toggle"
  );
  closePanel.click();
}

async function runTests(tab, monitor, document, isPrivate = false) {
  info("Open the panel again to see if the content is still there");
  const waitForPanels = waitFor(
    () =>
      document.querySelector(".http-custom-request-panel") &&
      document.querySelector("#http-custom-request-send-button").disabled ===
        isPrivate
  );

  const HTTPCustomRequestButton = document.querySelector(
    "#netmonitor-toolbar-container .devtools-http-custom-request-icon"
  );
  HTTPCustomRequestButton.click();
  await waitForPanels;

  // Wait a few seconds to make sure all the fields have been updated
  await wait(1500);

  const customMethod = document.querySelector("#http-custom-method-value");
  const customUrl = document.querySelector(".http-custom-url-value");
  const customQuery = document.querySelectorAll(
    "#http-custom-query .tabpanel-summary-container.http-custom-input textarea"
  );
  const customHeaders = document.querySelectorAll(
    "#http-custom-headers .tabpanel-summary-container.http-custom-input textarea"
  );
  const postDataValue = document.querySelector("#http-custom-postdata-value");

  if (isPrivate) {
    is(
      customMethod.value,
      "GET",
      "The method should not be persisted after the user close the panel and re-opened in PBM"
    );

    is(
      customUrl.value,
      "",
      "The url should not be there after the user close the panel and re-opened in PBM"
    );

    is(
      customQuery.length,
      0,
      "The Parameter should not be there after the user close the panel and re-opened in PBM"
    );

    is(
      customHeaders.length,
      0,
      "There should be no custom headers after the user close the panel and re-opened in PBM"
    );

    is(
      postDataValue.value,
      "",
      "The post data should still be reset after the user close the panel and re-opened in PBM"
    );
  } else {
    is(
      customMethod.value,
      "POST",
      "The method should be persisted after the user close the panel and re-opened"
    );

    is(
      customUrl.value,
      "https://www.example.com?My-param=",
      "The url should still be there after the user close the panel and re-opened"
    );

    const [nameParam] = Array.from(customQuery);
    is(
      nameParam.value,
      "My-param",
      "The Parameter name should still be there after the user close the panel and re-opened"
    );

    const [name, value] = Array.from(customHeaders);
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
      postDataValue.value,
      "{'Name': 'Value'}",
      "The content should still be there after the user close the panel and re-opened"
    );
  }
}

add_task(async function testRequestPanelPersistedContent() {
  // Turn true the pref
  await pushPref("devtools.netmonitor.features.newEditAndResend", true);
  // Reset the storage for the persisted custom request
  await asyncStorage.removeItem("devtools.netmonitor.customRequest");

  const { tab, monitor } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  info("Starting test... ");
  info("Add initial custom request test content");
  await addCustomRequestTestContent(tab, monitor, document);
  await runTests(tab, monitor, document);
  await teardown(monitor);
});

add_task(async function testRequestPanelPersistedContentInPrivateWindow() {
  await pushPref("devtools.netmonitor.features.newEditAndResend", true);
  const { tab, monitor, privateWindow } = await initNetMonitor(
    HTTPS_CUSTOM_GET_URL,
    {
      requestCount: 1,
      openInPrivateWindow: true,
    }
  );
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  info("Starting test in private window... ");
  await runTests(tab, monitor, document, true);
  await teardown(monitor, privateWindow);
});
