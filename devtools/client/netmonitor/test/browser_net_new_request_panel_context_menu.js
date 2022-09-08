/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const asyncStorage = require("devtools/shared/async-storage");

/**
 * Test if the New Request Panel shows up as a expected
 * when opened from an existing request
 */

add_task(async function() {
  // Turn true the pref
  await pushPref("devtools.netmonitor.features.newEditAndResend", true);
  // Reset the storage for the persisted custom request
  await asyncStorage.removeItem("devtools.netmonitor.customRequest");

  const { tab, monitor } = await initNetMonitor(POST_DATA_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire, connector } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 2);

  const { getSelectedRequest } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  const expectedURLQueryParams = [
    {
      name: "foo",
      value: "bar",
    },
    { name: "baz", value: "42" },
    { name: "type", value: "urlencoded" },
  ];

  info("selecting first request");
  const firstRequestItem = document.querySelectorAll(".request-list-item")[0];
  const waitForHeaders = waitUntil(() =>
    document.querySelector(".headers-overview")
  );
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequestItem);
  await waitForHeaders;
  EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequestItem);

  ok(
    getContextMenuItem(monitor, "request-list-context-resend-only"),
    "The 'Resend' item is visible when there is a clicked request"
  );

  info("Opening the new request panel");
  const waitForPanels = waitUntil(
    () =>
      document.querySelector(".http-custom-request-panel") &&
      document.querySelector("#http-custom-request-send-button").disabled ===
        false
  );

  await selectContextMenuItem(monitor, "request-list-context-edit-resend");
  await waitForPanels;

  is(
    !!document.querySelector(
      ".devtools-button.devtools-http-custom-request-icon.checked"
    ),
    true,
    "The toolbar button should be highlighted"
  );

  const request = getSelectedRequest(store.getState());

  // Verify if the default state contains the data from the request
  const methodValue = document.querySelector(".http-custom-method-value");
  is(
    methodValue.value,
    request.method,
    "The method in the form should match the request we clicked"
  );

  const urlValue = document.querySelector(".http-custom-url-value");
  is(
    urlValue.textContent,
    request.url,
    "The URL in the form should match the request we clicked"
  );

  const urlParametersValues = document.querySelectorAll(
    "#http-custom-query .tabpanel-summary-container.http-custom-input"
  );
  is(
    urlParametersValues.length,
    3,
    "The URL Parameters length in the form should match the request we clicked"
  );

  for (let i = 0; i < urlParametersValues.length; i++) {
    const { name, value } = expectedURLQueryParams[i];
    const [formName, formValue] = urlParametersValues[i].querySelectorAll(
      "textarea"
    );
    is(
      name,
      formName.value,
      "The query param name in the form should match the request we clicked"
    );
    is(
      value,
      formValue.value,
      "The query param value in the form should match the request we clicked"
    );
  }

  const headersValues = document.querySelectorAll(
    "#http-custom-headers .tabpanel-summary-container.http-custom-input"
  );
  ok(
    headersValues.length >= 6,
    "The headers length in the form should match the request we clicked"
  );

  for (const { name, value } of request.requestHeaders.headers) {
    const found = Array.from(headersValues).find(item => {
      const [formName, formValue] = item.querySelectorAll("textarea");
      if (formName.value === name && formValue.value === value) {
        return true;
      }
      return false;
    });

    ok(found, "The header was found in the form");
  }

  // Wait to the post body because it is only updated in the componentWillMount
  const postValue = document.querySelector("#http-custom-postdata-value");
  await waitUntil(() => postValue.textContent !== "");
  is(
    postValue.value,
    request.requestPostData.postData.text,
    "The Post body input value in the form should match the request we clicked"
  );

  info(
    "Uncheck the header an make sure the header is removed from the new request"
  );
  const headers = document.querySelectorAll(
    "#http-custom-headers .tabpanel-summary-container.http-custom-input"
  );

  const lastHeader = Array.from(headers).pop();
  const checkbox = lastHeader.querySelector("input");
  checkbox.click();

  info("Click on the button to send a new request");
  const waitUntilEventsDisplayed = waitForNetworkEvents(monitor, 1);
  const buttonSend = document.querySelector("#http-custom-request-send-button");
  buttonSend.click();
  await waitUntilEventsDisplayed;

  const newRequestSelectedId = getSelectedRequest(store.getState()).id;
  await connector.requestData(newRequestSelectedId, "requestHeaders");
  const updatedSelectedRequest = getSelectedRequest(store.getState());

  let found = updatedSelectedRequest.requestHeaders.headers.some(
    item => item.name == "My-header-2" && item.value == "my-value-2"
  );

  is(
    found,
    false,
    "The header unchecked should not be found on the headers list"
  );

  info(
    "Delete the header and make sure the header is removed in the custom request panel"
  );
  const buttonDelete = lastHeader.querySelector("button");
  buttonDelete.click();

  const headersValue = document.querySelectorAll(
    "#http-custom-headers .tabpanel-summary-container.http-custom-input textarea"
  );
  found = Array.from(headersValue).some(
    item => item.name == "My-header-2" && item.value == "my-value-2"
  );
  is(found, false, "The header delete should not be found on the headers form");

  info(
    "Change the request selected to make sure the request in the custom request panel does not change"
  );
  const previousRequest = document.querySelectorAll(".request-list-item")[0];
  EventUtils.sendMouseEvent({ type: "mousedown" }, previousRequest);

  const urlValueChanged = document.querySelector(".http-custom-url-value");
  is(
    urlValue.textContent,
    urlValueChanged.textContent,
    "The url should not change when click on a new request"
  );

  await teardown(monitor);
});
