/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the POST requests display the correct information in the UI,
 * for JSON payloads.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { tab, monitor } = await initNetMonitor(POST_JSON_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 1);

  // Wait for all aaccordion items updated by react, Bug 1514750 - wait for editor also
  const waitAccordionItems = waitForDOM(
    document,
    "#request-panel .accordion-item",
    2
  );
  const waitSourceEditor = waitForDOM(
    document,
    "#request-panel .CodeMirror-code"
  );
  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#request-tab")
  );
  await Promise.all([waitAccordionItems, waitSourceEditor]);
  const tabpanel = document.querySelector("#request-panel");

  ok(
    tabpanel.querySelector(".treeTable"),
    "The request params doesn't have the indended visibility."
  );
  // Bug 1514750 - Show JSON request in plain text view also
  ok(
    tabpanel.querySelector(".CodeMirror-code"),
    "The request post data doesn't have the indended visibility."
  );
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

  const accordionItems = tabpanel.querySelectorAll(".accordion-item");

  is(
    accordionItems[0].querySelector(".accordion-header-label").textContent,
    L10N.getStr("jsonScopeName"),
    "The post section doesn't have the correct title."
  );
  is(
    accordionItems[1].querySelector(".accordion-header-label").textContent,
    L10N.getStr("paramsPostPayload"),
    "The post section doesn't have the correct title."
  );

  const labels = tabpanel.querySelectorAll("tr .treeLabelCell .treeLabel");
  const values = tabpanel.querySelectorAll("tr .treeValueCell .objectBox");

  is(labels[0].textContent, "a", "The JSON var name was incorrect.");
  is(values[0].textContent, "1", "The JSON var value was incorrect.");

  ok(
    getCodeMirrorValue(monitor).includes('{"a":1}'),
    "The text shown in the source editor is incorrect."
  );

  return teardown(monitor);
});
