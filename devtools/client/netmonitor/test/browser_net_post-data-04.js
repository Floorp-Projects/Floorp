/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the POST requests display the correct information in the UI,
 * for JSON payloads.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { tab, monitor } = await initNetMonitor(POST_JSON_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 1);

  // Wait for all tree view updated by react
  wait = waitForDOM(document, "#params-panel .tree-section");
  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#params-tab"));
  await wait;

  const tabpanel = document.querySelector("#params-panel");

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

  const labels = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
  const values = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

  is(labels[0].textContent, "a", "The JSON var name was incorrect.");
  is(values[0].textContent, "1", "The JSON var value was incorrect.");

  return teardown(monitor);
});
