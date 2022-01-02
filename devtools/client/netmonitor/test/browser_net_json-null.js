/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSON responses containing null values are properly displayed.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(JSON_BASIC_URL + "?name=null", {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 1);

  const onResponsePanelReady = waitForDOM(
    document,
    "#response-panel .data-header"
  );

  const onPropsViewReady = waitForDOM(
    document,
    "#response-panel .properties-view",
    1
  );
  store.dispatch(Actions.toggleNetworkDetails());
  clickOnSidebarTab(document, "response");
  await Promise.all([onResponsePanelReady, onPropsViewReady]);

  const tabpanel = document.querySelector("#response-panel");
  is(
    tabpanel.querySelectorAll(".raw-data-toggle").length,
    1,
    "There should be 1 raw response toggle."
  );
  is(
    tabpanel.querySelectorAll(".treeRow").length,
    1,
    "There should be 1 json properties displayed in this tabpanel."
  );
  is(
    tabpanel.querySelectorAll(".empty-notice").length,
    0,
    "The empty notice should not be displayed in this tabpanel."
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
    "null",
    "The first json property value was incorrect."
  );

  const onCodeMirrorReady = waitForDOM(
    document,
    "#response-panel .CodeMirror-code"
  );

  const rawResponseToggle = document.querySelector(
    "#response-panel .raw-data-toggle-input .devtools-checkbox-toggle"
  );
  clickElement(rawResponseToggle, monitor);

  await onCodeMirrorReady;

  checkResponsePanelDisplaysJSON();

  await teardown(monitor);

  /**
   * Helper to assert that the response panel found in the provided document is currently
   * showing a preview of a JSON object.
   */
  function checkResponsePanelDisplaysJSON() {
    const panel = document.querySelector("#response-panel");
    is(
      panel.querySelector(".response-error-header") === null,
      true,
      "The response error header doesn't have the intended visibility."
    );
    const jsonView = panel.querySelector(".data-label") || {};
    is(
      jsonView.textContent === L10N.getStr("jsonScopeName"),
      true,
      "The response json view has the intended visibility."
    );
    is(
      panel.querySelector(".CodeMirror-code") === null,
      false,
      "The response editor has the intended visibility."
    );
    is(
      panel.querySelector(".response-image-box") === null,
      true,
      "The response image box doesn't have the intended visibility."
    );
  }
});
