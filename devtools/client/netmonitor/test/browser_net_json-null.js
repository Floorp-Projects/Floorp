/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { L10N } = require("devtools/client/netmonitor/l10n");

/**
 * Tests if JSON responses containing null values are properly displayed.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(JSON_BASIC_URL + "?name=null");
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  yield openResponsePanel(document);
  checkResponsePanelDisplaysJSON(document);

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
});

/**
 * Helper to assert that the response panel found in the provided document is currently
 * showing a preview of a JSON object.
 */
function checkResponsePanelDisplaysJSON(doc) {
  let tabpanel = doc.querySelector("#response-panel");
  is(tabpanel.querySelector(".response-error-header") === null, true,
    "The response error header doesn't have the intended visibility.");
  let jsonView = tabpanel.querySelector(".tree-section .treeLabel") || {};
  is(jsonView.textContent === L10N.getStr("jsonScopeName"), true,
    "The response json view has the intended visibility.");
  is(tabpanel.querySelector(".editor-mount iframe") === null, true,
    "The response editor doesn't have the intended visibility.");
  is(tabpanel.querySelector(".response-image-box") === null, true,
    "The response image box doesn't have the intended visibility.");
}

/**
 * Open the netmonitor details panel and switch to the response tab.
 * Returns a promise that will resolve when the response panel DOM element is available.
 */
function openResponsePanel(doc) {
  let onReponsePanelReady = waitForDOM(doc, "#response-panel");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    doc.getElementById("details-pane-toggle")
  );
  doc.querySelector("#response-tab").click();
  return onReponsePanelReady;
}
