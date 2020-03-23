/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if empty JSON responses are properly displayed.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(
    JSON_EMPTY_URL + "?name=empty",
    { requestCount: 1 }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 1);

  const onResponsePanelReady = waitForDOM(
    document,
    "#response-panel .accordion-item",
    2
  );

  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#response-tab")
  );

  await onResponsePanelReady;

  const codeMirrorReady = waitForDOM(
    document,
    "#response-panel .CodeMirror-code"
  );

  const header = document.querySelector(
    "#response-panel .accordion-item:last-child .accordion-header"
  );
  clickElement(header, monitor);

  await codeMirrorReady;

  const tabpanel = document.querySelector("#response-panel");
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
    tabpanel.querySelector(".response-error-header") === null,
    true,
    "The response error header doesn't have the intended visibility."
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

  const jsonView =
    tabpanel.querySelector(".accordion-item .accordion-header-label") || {};
  is(
    jsonView.textContent === L10N.getStr("jsonScopeName"),
    true,
    "The response json view has the intended visibility."
  );

  await teardown(monitor);
});
