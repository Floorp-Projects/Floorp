/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
  * Bug 1542172 -
  * Verifies that requests with large post data are truncated and error is displayed.
*/
add_task(async function() {
  const { monitor, tab } = await initNetMonitor(POST_JSON_URL);

  info("Starting test... ");

  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  requestLongerTimeout(2);

  info("Perform requests");
  await performRequestsAndWait();

  await waitUntil(() => document.querySelector(".request-list-item"));
  const item = document.querySelectorAll(".request-list-item")[0];
  await waitUntil(() => item.querySelector(".requests-list-type").title);

  wait = waitForDOM(document, "#params-panel .tree-section", 1);
  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent({ type: "click" }, document.querySelector("#params-tab"));
  await wait;

  const tabpanel = document.querySelector("#params-panel");
  is(tabpanel.querySelector(".request-error-header") === null, false,
    "The request error header doesn't have the intended visibility.");
  is(tabpanel.querySelector(".request-error-header").textContent,
    "Request has been truncated", "The error message shown is incorrect");
  const jsonView = tabpanel.querySelector(".tree-section .treeLabel") || {};
  is(jsonView.textContent === L10N.getStr("jsonScopeName"), false,
    "The params json view doesn't have the intended visibility.");
  is(tabpanel.querySelector("pre") === null, false,
    "The Request Payload has the intended visibility.");

  return teardown(monitor);
  async function performRequestsAndWait() {
    const wait = waitForNetworkEvents(monitor, 1);
    await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
      content.wrappedJSObject.performLargePostDataRequest();
    });
    await wait;
  }
});
