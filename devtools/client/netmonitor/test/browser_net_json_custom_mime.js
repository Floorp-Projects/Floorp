/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSON responses with unusal/custom MIME types are handled correctly.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(JSON_CUSTOM_MIME_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 1);

  const requestItem = document.querySelector(".request-list-item");
  const requestsListStatus = requestItem.querySelector(".status-code");
  EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
  await waitUntil(() => requestsListStatus.title);
  await waitForDOMIfNeeded(requestItem, ".requests-list-timings-total");

  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[0],
    "GET",
    CONTENT_TYPE_SJS + "?fmt=json-custom-mime",
    {
      status: 200,
      statusText: "OK",
      type: "x-bigcorp-json",
      fullMimeType: "text/x-bigcorp-json; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 41),
      time: true,
    }
  );

  let wait = waitForDOM(document, "#response-panel .data-header");
  const waitForPropsView = waitForDOM(
    document,
    "#response-panel .properties-view",
    1
  );

  store.dispatch(Actions.toggleNetworkDetails());
  clickOnSidebarTab(document, "response");
  await Promise.all([wait, waitForPropsView]);

  testJsonSectionInResponseTab();

  wait = waitForDOM(document, "#response-panel .CodeMirror-code");
  const rawResponseToggle = document.querySelector(
    "#response-panel .raw-data-toggle-input .devtools-checkbox-toggle"
  );
  clickElement(rawResponseToggle, monitor);
  await wait;

  testResponseTab();

  await teardown(monitor);

  function testJsonSectionInResponseTab() {
    const tabpanel = document.querySelector("#response-panel");
    is(
      tabpanel.querySelectorAll(".treeRow").length,
      1,
      "There should be 1 json properties displayed in this tabpanel."
    );

    const labels = tabpanel.querySelectorAll("tr .treeLabelCell .treeLabel");
    const values = tabpanel.querySelectorAll("tr .treeValueCell .objectBox");

    is(
      labels[0].textContent,
      "greeting",
      "The first json property name was incorrect."
    );
    is(
      values[0].textContent,
      `"Hello oddly-named JSON!"`,
      "The first json property value was incorrect."
    );
  }

  function testResponseTab() {
    const tabpanel = document.querySelector("#response-panel");

    is(
      tabpanel.querySelector(".response-error-header") === null,
      true,
      "The response error header doesn't have the intended visibility."
    );
    const jsonView = tabpanel.querySelector(".data-label") || {};
    is(
      jsonView.textContent === L10N.getStr("jsonScopeName"),
      true,
      "The response json view has the intended visibility."
    );
    is(
      tabpanel.querySelector(".raw-data-toggle-input .devtools-checkbox-toggle")
        .checked,
      true,
      "The raw response toggle should be on."
    );
    is(
      tabpanel.querySelector(".CodeMirror-code") === null,
      false,
      "The response editor has the intended visibility."
    );
    is(
      tabpanel.querySelector(".response-image-box") === null,
      true,
      "The response image box doesn't have the intended visibility."
    );
    is(
      tabpanel.querySelectorAll(".empty-notice").length,
      0,
      "The empty notice should not be displayed in this tabpanel."
    );
  }
});
