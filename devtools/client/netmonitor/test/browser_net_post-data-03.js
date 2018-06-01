/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the POST requests display the correct information in the UI,
 * for raw payloads with content-type headers attached to the upload stream.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { tab, monitor } = await initNetMonitor(POST_RAW_WITH_HEADERS_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 1);

  // Wait for all tree view updated by react
  wait = waitForDOM(document, "#headers-panel .tree-section .treeLabel", 3);
  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#headers-tab"));
  await wait;

  let tabpanel = document.querySelector("#headers-panel");
  is(tabpanel.querySelectorAll(".tree-section .treeLabel").length, 3,
    "There should be 3 header sections displayed in this tabpanel.");

  is(tabpanel.querySelectorAll(".tree-section .treeLabel")[2].textContent,
    L10N.getStr("requestHeadersFromUpload") + " (" +
    L10N.getFormatStr("networkMenu.sizeB", 74) + ")",
    "The request headers from upload section doesn't have the correct title.");

  let labels = tabpanel
    .querySelectorAll(".properties-view tr:not(.tree-section) .treeLabelCell .treeLabel");
  let values = tabpanel
    .querySelectorAll(".properties-view tr:not(.tree-section) .treeValueCell .objectBox");

  is(labels[labels.length - 2].textContent, "content-type",
    "The first request header name was incorrect.");
  is(values[values.length - 2].textContent, "application/x-www-form-urlencoded",
    "The first request header value was incorrect.");
  is(labels[labels.length - 1].textContent, "custom-header",
    "The second request header name was incorrect.");
  is(values[values.length - 1].textContent, "hello world!",
    "The second request header value was incorrect.");

  // Wait for all tree sections updated by react
  wait = waitForDOM(document, "#params-panel .tree-section");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#params-tab"));
  await wait;

  tabpanel = document.querySelector("#params-panel");

  ok(tabpanel.querySelector(".treeTable"),
    "The params tree view should be displayed.");
  ok(tabpanel.querySelector(".editor-mount") === null,
    "The post data shouldn't be displayed.");

  is(tabpanel.querySelector(".tree-section .treeLabel").textContent,
    L10N.getStr("paramsFormData"),
    "The form data section doesn't have the correct title.");

  labels = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
  values = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

  is(labels[0].textContent, "baz", "The first payload param name was incorrect.");
  is(values[0].textContent, "123", "The first payload param value was incorrect.");
  is(labels[1].textContent, "foo", "The second payload param name was incorrect.");
  is(values[1].textContent, "bar", "The second payload param value was incorrect.");

  return teardown(monitor);
});
