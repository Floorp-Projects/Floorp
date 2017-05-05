/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the POST requests display the correct information in the UI,
 * for raw payloads with attached content-type headers.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  let { tab, monitor } = yield initNetMonitor(POST_RAW_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 0, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  // Wait for all tree view updated by react
  wait = waitForDOM(document, "#params-panel .tree-section");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#params-tab"));
  yield wait;

  let tabpanel = document.querySelector("#params-panel");

  ok(tabpanel.querySelector(".treeTable"),
    "The request params doesn't have the indended visibility.");
  ok(tabpanel.querySelector(".editor-mount") === null,
    "The request post data doesn't have the indended visibility.");

  is(tabpanel.querySelectorAll(".tree-section").length, 1,
    "There should be 1 tree sections displayed in this tabpanel.");
  is(tabpanel.querySelectorAll(".empty-notice").length, 0,
    "The empty notice should not be displayed in this tabpanel.");

  is(tabpanel.querySelector(".tree-section .treeLabel").textContent,
    L10N.getStr("paramsFormData"),
    "The post section doesn't have the correct title.");

  let labels = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
  let values = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

  is(labels[0].textContent, "foo", "The first query param name was incorrect.");
  is(values[0].textContent, "bar", "The first query param value was incorrect.");
  is(labels[1].textContent, "baz", "The second query param name was incorrect.");
  is(values[1].textContent, "123", "The second query param value was incorrect.");

  return teardown(monitor);
});
