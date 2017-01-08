/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the POST requests display the correct information in the UI,
 * for raw payloads with content-type headers attached to the upload stream.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  let { tab, monitor } = yield initNetMonitor(POST_RAW_WITH_HEADERS_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 0, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  // Wait for all tree view updated by react
  wait = waitForDOM(document, "#headers-tabpanel .variables-view-scope", 3);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[0]);
  yield wait;

  let tabEl = document.querySelectorAll("#details-pane tab")[0];
  let tabpanel = document.querySelectorAll("#details-pane tabpanel")[0];
  let requestFromUploadScope = tabpanel.querySelectorAll(".variables-view-scope")[2];

  is(tabEl.getAttribute("selected"), "true",
    "The headers tab in the network details pane should be selected.");
  is(tabpanel.querySelectorAll(".variables-view-scope").length, 3,
    "There should be 3 header scopes displayed in this tabpanel.");

  is(requestFromUploadScope.querySelector(".name").getAttribute("value"),
    L10N.getStr("requestHeadersFromUpload") + " (" +
    L10N.getFormatStr("networkMenu.sizeKB", L10N.numberWithDecimals(74 / 1024, 3)) + ")",
    "The request headers from upload scope doesn't have the correct title.");

  is(requestFromUploadScope.querySelectorAll(".variables-view-variable").length, 2,
    "There should be 2 headers displayed in the request headers from upload scope.");

  is(requestFromUploadScope.querySelectorAll(".variables-view-variable .name")[0]
    .getAttribute("value"),
    "content-type", "The first request header name was incorrect.");
  is(requestFromUploadScope.querySelectorAll(".variables-view-variable .value")[0]
    .getAttribute("value"), "\"application/x-www-form-urlencoded\"",
    "The first request header value was incorrect.");
  is(requestFromUploadScope.querySelectorAll(".variables-view-variable .name")[1]
    .getAttribute("value"),
    "custom-header", "The second request header name was incorrect.");
  is(requestFromUploadScope.querySelectorAll(".variables-view-variable .value")[1]
    .getAttribute("value"),
    "\"hello world!\"", "The second request header value was incorrect.");

  // Wait for all tree sections updated by react
  wait = waitForDOM(document, "#params-tabpanel .tree-section");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[2]);
  yield wait;

  tabpanel = document.querySelectorAll("#details-pane tabpanel")[2];

  ok(tabpanel.querySelector(".treeTable"),
    "The params tree view should be displayed.");
  ok(tabpanel.querySelector(".editor-mount") === null,
    "The post data shouldn't be displayed.");

  is(tabpanel.querySelector(".tree-section .treeLabel").textContent,
    L10N.getStr("paramsFormData"),
    "The form data section doesn't have the correct title.");

  let labels = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
  let values = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

  is(labels[0].textContent, "foo", "The first payload param name was incorrect.");
  is(values[0].textContent, "\"bar\"", "The first payload param value was incorrect.");
  is(labels[1].textContent, "baz", "The second payload param name was incorrect.");
  is(values[1].textContent, "\"123\"", "The second payload param value was incorrect.");

  return teardown(monitor);
});
