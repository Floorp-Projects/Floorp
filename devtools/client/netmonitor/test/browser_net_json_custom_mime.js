/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSON responses with unusal/custom MIME types are handled correctly.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  let { tab, monitor } = yield initNetMonitor(JSON_CUSTOM_MIME_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  verifyRequestItemTarget(RequestsMenu, RequestsMenu.getItemAtIndex(0),
    "GET", CONTENT_TYPE_SJS + "?fmt=json-custom-mime", {
      status: 200,
      statusText: "OK",
      type: "x-bigcorp-json",
      fullMimeType: "text/x-bigcorp-json; charset=utf-8",
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 41),
      time: true
    });

  wait = waitForDOM(document, "#panel-3");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  document.querySelector("#tab-3 a").click();
  yield wait;

  testResponseTab();

  yield teardown(monitor);

  function testResponseTab() {
    let tabpanel = document.querySelector("#panel-3");

    is(tabpanel.querySelector(".response-error-header") === null, true,
      "The response error header doesn't have the intended visibility.");
    let jsonView = tabpanel.querySelector(".tree-section .treeLabel") || {};
    is(jsonView.textContent === L10N.getStr("jsonScopeName"), true,
      "The response json view has the intended visibility.");
    is(tabpanel.querySelector(".editor-mount iframe") === null, true,
      "The response editor doesn't have the intended visibility.");
    is(tabpanel.querySelector(".response-image-box") === null, true,
      "The response image box doesn't have the intended visibility.");

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

    is(labels[0].textContent, "greeting",
      "The first json property name was incorrect.");
    is(values[0].textContent, "\"Hello oddly-named JSON!\"",
      "The first json property value was incorrect.");
  }
});
