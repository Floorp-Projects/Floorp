/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const asyncStorage = require("devtools/shared/async-storage");

/**
 * Test sending a custom request.
 */
add_task(async function() {
  // Turn on the pref
  await pushPref("devtools.netmonitor.features.newEditAndResend", true);
  // Reset the storage for the persisted custom request
  await asyncStorage.removeItem("devtools.netmonitor.customRequest");

  const { monitor } = await initNetMonitor(CORS_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire, connector } = monitor.panelWin;
  const { sendHTTPRequest } = connector;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const { getSelectedRequest } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  // Build request with known information so that we can check later
  const request = {
    url: "https://test1.example.com" + CORS_SJS_PATH,
    method: "POST",
    headers: [
      { name: "Host", value: "fakehost.example.com" },
      { name: "User-Agent", value: "Testzilla" },
      { name: "Referer", value: "http://example.com/referrer" },
      { name: "Accept", value: "application/jarda" },
      { name: "Accept-Encoding", value: "compress, identity, funcoding" },
      { name: "Accept-Language", value: "cs-CZ" },
    ],
    body: "Hello",
    cause: {
      loadingDocumentUri: "http://example.com",
      stacktraceAvailable: true,
      type: "xhr",
    },
  };
  const waitUntilRequestDisplayed = waitForNetworkEvents(monitor, 1);
  sendHTTPRequest(request);
  await waitUntilRequestDisplayed;

  info("selecting first request");
  const firstRequestItem = document.querySelectorAll(".request-list-item")[0];
  const waitForHeaders = waitUntil(() =>
    document.querySelector(".headers-overview")
  );
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequestItem);
  await waitForHeaders;
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

  info(
    "Change the request method to send a new custom request with a different method"
  );
  const methodValue = document.querySelector("#http-custom-method-value");
  methodValue.value = "PUT";
  methodValue.dispatchEvent(new Event("change", { bubbles: true }));

  info("Change the URL to send a custom request with a different URL");
  const urlValue = document.querySelector(".http-custom-url-value");
  urlValue.focus();
  urlValue.value = "";
  EventUtils.sendString(`${request.url}?hello=world`);

  info("Check if the parameter section was updated");
  is(
    document.querySelectorAll(
      "#http-custom-query .tabpanel-summary-container.http-custom-input"
    ).length,
    1,
    "The parameter section should be updated"
  );

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
  postValue.value = "";
  EventUtils.sendString("{'name': 'value'}");

  // Close the details panel to see if after sending a new request
  // this request will be selected by default and
  // if the deitails panel will be open automatically.
  const waitForDetailsPanelToClose = waitUntil(
    () => !document.querySelector(".monitor-panel .network-details-bar")
  );
  store.dispatch(Actions.toggleNetworkDetails());
  await waitForDetailsPanelToClose;

  info("Click on the button to send a new request");
  const waitUntilEventsDisplayed = waitForNetworkEvents(monitor, 1);
  const buttonSend = document.querySelector("#http-custom-request-send-button");
  buttonSend.click();
  await waitUntilEventsDisplayed;

  const newRequestSelectedId = getSelectedRequest(store.getState()).id;
  await connector.requestData(newRequestSelectedId, "requestPostData");
  const updatedSelectedRequest = getSelectedRequest(store.getState());
  is(updatedSelectedRequest.method, "PUT", "The request has the right method");
  is(
    updatedSelectedRequest.url,
    urlValue.value,
    "The request has the right URL"
  );

  const found = updatedSelectedRequest.requestHeaders.headers.some(
    item => item.name === "My-header" && item.value === "my-value"
  );

  is(found, true, "The header was found in the form");

  is(
    updatedSelectedRequest.requestPostData.postData.text,
    "{'name': 'value'}",
    "The request has the right post body"
  );

  info("Check that all growing textareas provide a title tooltip");
  const textareas = [
    ...document.querySelectorAll("#http-custom-headers .auto-growing-textarea"),
  ];
  for (const textarea of textareas) {
    is(
      textarea.title,
      textarea.dataset.replicatedValue,
      "Title tooltip is set to the expected value"
    );
  }

  await teardown(monitor);
});
