/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const asyncStorage = require("devtools/shared/async-storage");

/**
 * Test to check the sync between URL parameters and the parameters section
 */

add_task(async function() {
  // Turn on the pref
  await pushPref("devtools.netmonitor.features.newEditAndResend", true);
  // Reset the storage for the persisted custom request
  await asyncStorage.removeItem("devtools.netmonitor.customRequest");

  const { tab, monitor } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 1);

  info("selecting first request");
  const firstRequestItem = document.querySelectorAll(".request-list-item")[0];
  const waitForHeaders = waitUntil(() =>
    document.querySelector(".headers-overview")
  );
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequestItem);
  await waitForHeaders;
  EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequestItem);

  info("Opening the new request panel");
  const waitForPanels = waitForDOM(
    document,
    ".monitor-panel .network-action-bar"
  );
  await selectContextMenuItem(monitor, "request-list-context-edit-resend");
  await waitForPanels;

  const queryScenarios = [
    {
      queryString: "",
      expectedParametersSize: 0,
      expectedParameters: [],
    },
    {
      queryString: "?",
      expectedParametersSize: 0,
      expectedParameters: [],
    },
    {
      queryString: "?a",
      expectedParametersSize: 1,
      expectedParameters: [{ name: "a", value: "" }],
    },
    {
      queryString: "?a=",
      expectedParametersSize: 1,
      expectedParameters: [{ name: "a", value: "" }],
    },
    {
      queryString: "?a=3",
      expectedParametersSize: 1,
      expectedParameters: [{ name: "a", value: "3" }],
    },
    {
      queryString: "?a=3&",
      expectedParametersSize: 2,
      expectedParameters: [
        { name: "a", value: "3" },
        { name: "", value: "" },
      ],
    },
    {
      queryString: "?a=3&b=4",
      expectedParametersSize: 2,
      expectedParameters: [
        { name: "a", value: "3" },
        { name: "b", value: "4" },
      ],
    },
  ];

  for (const sceanario of queryScenarios) {
    assertQueryScenario(document, sceanario);
  }

  info("Adding new parameters by query parameters section");
  const newParameterName = document.querySelector(
    "#http-custom-query .map-add-new-inputs .http-custom-input-name"
  );
  newParameterName.focus();
  EventUtils.sendString("My-param");

  is(
    document.querySelector("#http-custom-url-value").value,
    `${HTTPS_CUSTOM_GET_URL}?a=3&b=4&My-param=`,
    "The URL should be updated"
  );

  const newParameterValue = Array.from(
    document.querySelectorAll(
      "#http-custom-query .http-custom-input .http-custom-input-value"
    )
  ).pop();
  newParameterValue.focus();
  EventUtils.sendString("my-value");

  // Check if the url is updated
  is(
    document.querySelector("#http-custom-url-value").value,
    `${HTTPS_CUSTOM_GET_URL}?a=3&b=4&My-param=my-value`,
    "The URL should be updated"
  );

  info("Adding new parameters by query parameters section");
  is(
    document.querySelectorAll(
      "#http-custom-query .tabpanel-summary-container.http-custom-input"
    ).length,
    3,
    "The parameter section should be have 3 elements"
  );

  info(
    "Uncheck the parameter an make sure the parameter is removed from the new url"
  );
  const params = document.querySelectorAll(
    "#http-custom-query  .tabpanel-summary-container.http-custom-input"
  );

  const lastParam = Array.from(params).pop();
  const checkbox = lastParam.querySelector("input");
  checkbox.click();

  // Check if the url is updated after uncheck one parameter through the parameter section
  is(
    document.querySelector("#http-custom-url-value").value,
    `${HTTPS_CUSTOM_GET_URL}?a=3&b=4`,
    "The URL should be updated"
  );

  await teardown(monitor);
});

function assertQueryScenario(
  document,
  { queryString, expectedParametersSize, expectedParameters }
) {
  info(`Assert that "${queryString}" shows in the list properly`);
  const urlValue = document.querySelector(".http-custom-url-value");
  urlValue.focus();
  urlValue.value = "";

  const newURL = HTTPS_CUSTOM_GET_URL + queryString;
  EventUtils.sendString(newURL);

  is(
    document.querySelectorAll(
      "#http-custom-query .tabpanel-summary-container.http-custom-input"
    ).length,
    expectedParametersSize,
    `The parameter section should have ${expectedParametersSize} elements`
  );

  // Check if the parameter name and value are what we expect
  const parameterNames = document.querySelectorAll(
    "#http-custom-query .http-custom-input-and-map-form .http-custom-input-name"
  );
  const parameterValues = document.querySelectorAll(
    "#http-custom-query .http-custom-input-and-map-form .http-custom-input-value"
  );

  for (let i = 0; i < expectedParameters.length; i++) {
    is(
      parameterNames[i].value,
      expectedParameters[i].name,
      "The query param name in the form should match on the URL"
    );
    is(
      parameterValues[i].value,
      expectedParameters[i].value,
      "The query param value in the form should match on the URL"
    );
  }
}
