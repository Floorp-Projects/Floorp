/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if very long JSON responses are handled correctly.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { tab, monitor } = await initNetMonitor(JSON_LONG_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  // This is receiving over 80 KB of json and will populate over 6000 items
  // in a variables view instance. Debug builds are slow.
  requestLongerTimeout(4);

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
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
    CONTENT_TYPE_SJS + "?fmt=json-long",
    {
      status: 200,
      statusText: "OK",
      type: "json",
      fullMimeType: "text/json; charset=utf-8",
      size: L10N.getFormatStr(
        "networkMenu.sizeKB",
        L10N.numberWithDecimals(85975 / 1024, 2)
      ),
      time: true,
    }
  );

  let wait = waitForDOM(document, "#response-panel .accordion-item", 2);
  const waitForPropsView = waitForDOM(
    document,
    "#response-panel .properties-view",
    1
  );

  store.dispatch(Actions.toggleNetworkDetails());

  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#response-tab")
  );

  await Promise.all([wait, waitForPropsView]);

  testJsonAccordionInResposeTab();

  wait = waitForDOM(document, "#response-panel .CodeMirror-code");
  const payloadHeader = document.querySelector(
    "#response-panel .accordion-item:last-child .accordion-header"
  );
  clickElement(payloadHeader, monitor);
  await wait;

  testResponseTab();

  await teardown(monitor);

  function testJsonAccordionInResposeTab() {
    const tabpanel = document.querySelector("#response-panel");
    is(
      tabpanel.querySelectorAll(".treeRow").length,
      2047,
      "There should be 2047 json properties displayed in this tabpanel."
    );

    const labels = tabpanel.querySelectorAll("tr .treeLabelCell .treeLabel");
    const values = tabpanel.querySelectorAll("tr .treeValueCell .objectBox");

    is(
      labels[0].textContent,
      "0",
      "The first json property name was incorrect."
    );
    is(
      values[0].textContent,
      'Object { greeting: "Hello long string JSON!" }',
      "The first json property value was incorrect."
    );

    is(
      labels[1].textContent,
      "1",
      "The second json property name was incorrect."
    );
    is(
      values[1].textContent,
      '"Hello long string JSON!"',
      "The second json property value was incorrect."
    );
  }

  function testResponseTab() {
    const tabpanel = document.querySelector("#response-panel");

    is(
      tabpanel.querySelector(".response-error-header") === null,
      true,
      "The response error header doesn't have the intended visibility."
    );
    const jsonView =
      tabpanel.querySelector(".accordion-item .accordion-header-label") || {};
    is(
      jsonView.textContent === L10N.getStr("jsonScopeName"),
      true,
      "The response json view has the intended visibility."
    );
    is(
      tabpanel.querySelector(".source-editor-mount").clientHeight !== 0,
      true,
      "The source editor container has visible height."
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
      tabpanel.querySelectorAll(".accordion-item").length,
      2,
      "There should be 2 accordion items displayed in this tabpanel."
    );
    is(
      tabpanel.querySelectorAll(".empty-notice").length,
      0,
      "The empty notice should not be displayed in this tabpanel."
    );

    is(
      tabpanel.querySelector(".accordion-item .accordion-header-label")
        .textContent,
      L10N.getStr("jsonScopeName"),
      "The json view section doesn't have the correct title."
    );
  }
});
