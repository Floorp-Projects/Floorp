/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the POST requests display the correct information in the UI,
 * for JSON payloads.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  let { tab, monitor } = yield initNetMonitor(POST_JSON_URL);
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
  wait = waitForDOM(document, "#params-tabpanel .tree-section");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.getElementById("details-pane-toggle"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll("#details-pane tab")[2]);
  yield wait;

  let tabpanel = document.querySelectorAll("#event-details-pane tabpanel")[2];

  ok(tabpanel.querySelector(".treeTable"),
    "The request params doesn't have the indended visibility.");
  ok(tabpanel.querySelector(".editor-mount") === null,
    "The request post data doesn't have the indended visibility.");

  is(tabpanel.querySelectorAll(".tree-section").length, 1,
    "There should be 1 tree sections displayed in this tabpanel.");
  is(tabpanel.querySelectorAll(".empty-notice").length, 0,
    "The empty notice should not be displayed in this tabpanel.");

  is(tabpanel.querySelector(".tree-section .treeLabel").textContent,
    L10N.getStr("jsonScopeName"),
    "The JSON section doesn't have the correct title.");

  let labels = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
  let values = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

  is(labels[0].textContent, "a", "The JSON var name was incorrect.");
  is(values[0].textContent, "1", "The JSON var value was incorrect.");

  return teardown(monitor);
});
