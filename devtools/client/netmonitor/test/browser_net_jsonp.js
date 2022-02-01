/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSONP responses are handled correctly.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { tab, monitor } = await initNetMonitor(JSONP_URL, { requestCount: 1 });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 2);

  const requestItems = document.querySelectorAll(".request-list-item");
  for (const requestItem of requestItems) {
    requestItem.scrollIntoView();
    const requestsListStatus = requestItem.querySelector(".status-code");
    EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
    await waitUntil(() => requestsListStatus.title);
    await waitForDOMIfNeeded(requestItem, ".requests-list-timings-total");
  }

  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[0],
    "GET",
    CONTENT_TYPE_SJS + "?fmt=jsonp&jsonp=$_0123Fun",
    {
      status: 200,
      statusText: "OK",
      type: "json",
      fullMimeType: "text/json; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 41),
      time: true,
    }
  );
  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[1],
    "GET",
    CONTENT_TYPE_SJS + "?fmt=jsonp2&jsonp=$_4567Sad",
    {
      status: 200,
      statusText: "OK",
      type: "json",
      fullMimeType: "text/json; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 54),
      time: true,
    }
  );

  info("Testing first request");
  let wait = waitForDOM(document, "#response-panel .data-header");
  let waitForPropsView = waitForDOM(
    document,
    "#response-panel .properties-view",
    1
  );

  store.dispatch(Actions.toggleNetworkDetails());
  clickOnSidebarTab(document, "response");
  await Promise.all([wait, waitForPropsView]);

  testJsonSectionInResponseTab(`"Hello JSONP!"`);

  wait = waitForDOM(document, "#response-panel .CodeMirror-code");
  let rawResponseToggle = document.querySelector(
    "#response-panel .raw-data-toggle-input .devtools-checkbox-toggle"
  );
  clickElement(rawResponseToggle, monitor);
  await wait;

  testResponseTab("$_0123Fun");

  info("Testing second request");

  wait = waitForDOM(document, "#response-panel .data-header");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[1]
  );

  await wait;

  waitForPropsView = waitForDOM(
    document,
    "#response-panel .properties-view",
    1
  );
  rawResponseToggle = document.querySelector(
    "#response-panel .raw-data-toggle-input .devtools-checkbox-toggle"
  );
  clickElement(rawResponseToggle, monitor);

  await waitForPropsView;

  testJsonSectionInResponseTab(`"Hello weird JSONP!"`);

  wait = waitForDOM(document, "#response-panel .CodeMirror-code");
  rawResponseToggle = document.querySelector(
    "#response-panel .raw-data-toggle-input .devtools-checkbox-toggle"
  );
  clickElement(rawResponseToggle, monitor);
  await wait;

  testResponseTab("$_4567Sad");

  await teardown(monitor);

  function testJsonSectionInResponseTab(greeting) {
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
      greeting,
      "The first json property value was incorrect."
    );
  }

  function testResponseTab(func) {
    const tabpanel = document.querySelector("#response-panel");

    is(
      tabpanel.querySelector(".response-error-header") === null,
      true,
      "The response error header doesn't have the intended visibility."
    );
    is(
      tabpanel.querySelector(".data-label").textContent,
      L10N.getFormatStr("jsonpScopeName", func),
      "The response json view has the intened visibility and correct title."
    );
    is(
      tabpanel.querySelector(".CodeMirror-code") === null,
      false,
      "The response editor has the intended visibility."
    );
    is(
      tabpanel.querySelector(".responseImageBox") === null,
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
