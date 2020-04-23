/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1542172 -
 * Verifies that requests with large post data are truncated and error is displayed.
 */
add_task(async function() {
  const { monitor, tab } = await initNetMonitor(POST_JSON_URL, {
    requestCount: 1,
  });

  info("Starting test... ");

  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  requestLongerTimeout(2);

  info("Perform requests");
  await performRequestsAndWait(monitor, tab);

  await waitUntil(() => document.querySelector(".request-list-item"));
  const item = document.querySelectorAll(".request-list-item")[0];
  await waitUntil(() => item.querySelector(".requests-list-type").title);

  // Make sure the accordion items and editor is loaded
  const waitAccordionItems = waitForDOM(
    document,
    "#request-panel .accordion-item",
    1
  );
  const waitSourceEditor = waitForDOM(
    document,
    "#request-panel .CodeMirror.cm-s-mozilla"
  );

  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#request-tab")
  );

  await Promise.all([waitAccordionItems, waitSourceEditor]);

  const tabpanel = document.querySelector("#request-panel");
  is(
    tabpanel.querySelector(".request-error-header") === null,
    false,
    "The request error header doesn't have the intended visibility."
  );
  is(
    tabpanel.querySelector(".request-error-header").textContent,
    "Request has been truncated",
    "The error message shown is incorrect"
  );
  const jsonView =
    tabpanel.querySelector(".accordion-item .accordion-header-label") || {};
  is(
    jsonView.textContent === L10N.getStr("jsonScopeName"),
    false,
    "The params json view doesn't have the intended visibility."
  );
  is(
    tabpanel.querySelector("PRE") === null,
    false,
    "The Request Payload has the intended visibility."
  );

  return teardown(monitor);
});

async function performRequestsAndWait(monitor, tab) {
  const wait = waitForNetworkEvents(monitor, 1);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    content.wrappedJSObject.performLargePostDataRequest();
  });
  await wait;
}
