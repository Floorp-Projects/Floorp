/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSON responses containing null values are properly displayed.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(JSON_BASIC_URL + "?name=null");
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  yield openResponsePanel();
  checkResponsePanelDisplaysJSON();

  let tabpanel = document.querySelector("#response-panel");
  is(tabpanel.querySelectorAll(".tree-section").length, 1,
    "There should be 1 tree sections displayed in this tabpanel.");
  is(tabpanel.querySelectorAll(".treeRow:not(.tree-section)").length, 1,
    "There should be 1 json properties displayed in this tabpanel.");
  is(tabpanel.querySelectorAll(".empty-notice").length, 0,
    "The empty notice should not be displayed in this tabpanel.");

  let labels = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
  let values = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

  is(labels[0].textContent, "greeting", "The first json property name was incorrect.");
  is(values[0].textContent, "null", "The first json property value was incorrect.");

  yield teardown(monitor);

  /**
   * Helper to assert that the response panel found in the provided document is currently
   * showing a preview of a JSON object.
   */
  function checkResponsePanelDisplaysJSON() {
    let panel = document.querySelector("#response-panel");
    is(panel.querySelector(".response-error-header") === null, true,
      "The response error header doesn't have the intended visibility.");
    let jsonView = panel.querySelector(".tree-section .treeLabel") || {};
    is(jsonView.textContent === L10N.getStr("jsonScopeName"), true,
      "The response json view has the intended visibility.");
    is(panel.querySelector(".CodeMirror-code") === null, true,
      "The response editor doesn't have the intended visibility.");
    is(panel.querySelector(".response-image-box") === null, true,
      "The response image box doesn't have the intended visibility.");
  }

  /**
   * Open the netmonitor details panel and switch to the response tab.
   * Returns a promise that will resolve when the response panel DOM element is available.
   */
  function openResponsePanel() {
    let onReponsePanelReady = waitForDOM(document, "#response-panel");
    EventUtils.sendMouseEvent({ type: "click" },
      document.querySelector(".network-details-panel-toggle"));
    EventUtils.sendMouseEvent({ type: "click" },
      document.querySelector("#response-tab"));
    return onReponsePanelReady;
  }
});
