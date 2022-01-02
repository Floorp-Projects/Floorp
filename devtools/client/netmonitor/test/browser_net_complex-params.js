/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether complex request params and payload sent via POST are
 * displayed correctly.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(PARAMS_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 12);

  const requestListItems = document.querySelectorAll(
    ".network-monitor .request-list-item"
  );

  // Select the Request tab.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requestListItems[0]);
  clickOnSidebarTab(document, "request");

  await testRequestWithFormattedView(
    monitor,
    requestListItems[0],
    '{ "foo": "bar" }',
    "",
    '{ "foo": "bar" }',
    1
  );
  await testRequestWithFormattedView(
    monitor,
    requestListItems[1],
    '{ "foo": "bar" }',
    "",
    '{ "foo": "bar" }',
    1
  );
  await testRequestWithFormattedView(
    monitor,
    requestListItems[2],
    "?foo",
    "bar",
    "?foo=bar",
    1
  );
  await testRequestWithFormattedView(
    monitor,
    requestListItems[3],
    "foo",
    "bar",
    '{ "foo": "bar" }',
    2
  );
  await testRequestWithFormattedView(
    monitor,
    requestListItems[4],
    "foo",
    "bar",
    '{ "foo": "bar" }',
    2
  );
  await testRequestWithOnlyRawDataView(
    monitor,
    requestListItems[5],
    "?foo=bar"
  );
  await testRequestWithoutRequestData(monitor, requestListItems[6]);
  await testRequestWithFormattedView(
    monitor,
    requestListItems[7],
    '{ "foo": "bar" }',
    "",
    '{ "foo": "bar" }',
    1
  );
  await testRequestWithFormattedView(
    monitor,
    requestListItems[8],
    '{ "foo": "bar" }',
    "",
    '{ "foo": "bar" }',
    1
  );

  await teardown(monitor);
});

async function testRequestWithFormattedView(
  monitor,
  requestListItem,
  paramName,
  paramValue,
  rawValue,
  dataType
) {
  const { document, windowRequire } = monitor.panelWin;
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");

  // Wait for header and properties view to be displayed
  const wait = waitForDOM(document, "#request-panel .data-header");
  let waitForContent = waitForDOM(document, "#request-panel .properties-view");
  EventUtils.sendMouseEvent({ type: "mousedown" }, requestListItem);
  await Promise.all([wait, waitForContent]);

  const tabpanel = document.querySelector("#request-panel");
  let headerLabel;
  switch (dataType) {
    case 1:
      headerLabel = L10N.getStr("paramsFormData");
      break;

    case 2:
      headerLabel = L10N.getStr("jsonScopeName");
      break;
  }

  is(
    tabpanel.querySelectorAll(".raw-data-toggle").length,
    1,
    "The raw request data toggle should be displayed in this tabpanel."
  );
  is(
    tabpanel.querySelectorAll("tr.treeRow").length,
    1,
    "The number of param rows displayed in this tabpanel is incorrect."
  );
  ok(
    tabpanel.querySelector(".empty-notice") === null,
    "The empty notice should not be displayed in this tabpanel."
  );

  ok(
    tabpanel.querySelector(".treeTable"),
    "The request params box should be displayed."
  );
  ok(
    tabpanel.querySelector(".CodeMirror-code") === null,
    "The request post data editor should not be displayed."
  );

  const labels = tabpanel.querySelectorAll("tr .treeLabelCell .treeLabel");
  const values = tabpanel.querySelectorAll("tr .treeValueCell .objectBox");

  is(
    tabpanel.querySelector(".data-label").textContent,
    headerLabel,
    "The form data section doesn't have the correct title."
  );

  is(
    labels[0].textContent,
    paramName,
    "The first form data param name was incorrect."
  );
  is(
    values[0].textContent,
    `"${paramValue}"`,
    "The first form data param value was incorrect."
  );

  // Toggle the raw data display. This should hide the formatted display.
  waitForContent = waitForDOM(document, "#request-panel .CodeMirror-code");
  let rawDataToggle = document.querySelector(
    "#request-panel .raw-data-toggle-input .devtools-checkbox-toggle"
  );
  clickElement(rawDataToggle, monitor);
  await waitForContent;

  const dataLabel = tabpanel.querySelector(".data-label") ?? {};
  is(
    dataLabel.textContent,
    L10N.getStr("paramsPostPayload"),
    "The label for the raw request payload is correct."
  );
  is(
    tabpanel.querySelector(".raw-data-toggle-input .devtools-checkbox-toggle")
      .checked,
    true,
    "The raw request toggle should be on."
  );
  is(
    tabpanel.querySelector(".properties-view") === null,
    true,
    "The formatted display should be hidden."
  );
  is(
    tabpanel.querySelector(".CodeMirror-code") !== null,
    true,
    "The raw payload data display is shown."
  );
  is(
    getCodeMirrorValue(monitor),
    rawValue,
    "The raw payload data string needs to be correct."
  );
  ok(
    tabpanel.querySelector(".empty-notice") === null,
    "The empty notice should not be displayed in this tabpanel."
  );

  // Toggle the raw data display off again. This should show the formatted display.
  // This is required to reset the original state
  waitForContent = waitForDOM(document, "#request-panel .properties-view");
  rawDataToggle = document.querySelector(
    "#request-panel .raw-data-toggle-input .devtools-checkbox-toggle"
  );
  clickElement(rawDataToggle, monitor);
  await waitForContent;
}

async function testRequestWithOnlyRawDataView(
  monitor,
  requestListItem,
  paramName
) {
  const { document, windowRequire } = monitor.panelWin;
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");

  // Wait for header and CodeMirror editor to be displayed
  const wait = waitForDOM(document, "#request-panel .data-header");
  const waitForContent = waitForDOM(
    document,
    "#request-panel .CodeMirror-code"
  );
  EventUtils.sendMouseEvent({ type: "mousedown" }, requestListItem);
  await Promise.all([wait, waitForContent]);

  const tabpanel = document.querySelector("#request-panel");

  const dataLabel = tabpanel.querySelector(".data-label") ?? {};
  is(
    dataLabel.textContent,
    L10N.getStr("paramsPostPayload"),
    "The label for the raw request payload is correct."
  );
  is(
    tabpanel.querySelectorAll(".raw-data-toggle").length,
    0,
    "The raw request data toggle should not be displayed in this tabpanel."
  );
  is(
    tabpanel.querySelector(".properties-view") === null,
    true,
    "The formatted display should be hidden."
  );
  is(
    tabpanel.querySelector(".CodeMirror-code") !== null,
    true,
    "The raw payload data display is shown."
  );
  is(
    getCodeMirrorValue(monitor),
    paramName,
    "The raw payload data string needs to be correct."
  );
  ok(
    tabpanel.querySelector(".empty-notice") === null,
    "The empty notice should not be displayed in this tabpanel."
  );
}

async function testRequestWithoutRequestData(monitor, requestListItem) {
  const { document } = monitor.panelWin;

  EventUtils.sendMouseEvent({ type: "mousedown" }, requestListItem);

  const tabpanel = document.querySelector("#request-panel");

  is(
    tabpanel.querySelector(".data-label") === null,
    true,
    "There must be no label for the request payload."
  );
  is(
    tabpanel.querySelectorAll(".raw-data-toggle").length,
    0,
    "The raw request data toggle should not be displayed in this tabpanel."
  );
  is(
    tabpanel.querySelector(".properties-view") === null,
    true,
    "The formatted display should be hidden."
  );
  is(
    tabpanel.querySelector(".CodeMirror-code") === null,
    true,
    "The raw payload data display should be hidden."
  );
  is(
    tabpanel.querySelector(".empty-notice") !== null,
    true,
    "The empty notice should be displayed in this tabpanel."
  );
  is(
    tabpanel.querySelector(".empty-notice").textContent,
    L10N.getStr("paramsNoPayloadText"),
    "The empty notice should be correct."
  );
}
