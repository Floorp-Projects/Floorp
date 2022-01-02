/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSON responses encoded in base64 are handled correctly.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
  const { tab, monitor } = await initNetMonitor(JSON_B64_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 1);

  let wait = waitForDOM(document, "#response-panel .data-header");
  const waitForPropsView = waitForDOM(
    document,
    "#response-panel .properties-view",
    1
  );

  store.dispatch(Actions.toggleNetworkDetails());

  clickOnSidebarTab(document, "response");

  await Promise.all([wait, waitForPropsView]);

  const tabpanel = document.querySelector("#response-panel");
  is(
    tabpanel.querySelectorAll(".treeRow").length,
    1,
    "There should be 1 json properties displayed in this tabpanel."
  );

  const labels = tabpanel.querySelectorAll("tr .treeLabelCell .treeLabel");
  const values = tabpanel.querySelectorAll("tr .treeValueCell .objectBox");

  is(
    labels[0].textContent,
    "greeting",
    "The first json property name was incorrect."
  );
  is(
    values[0].textContent,
    `"This is a base 64 string."`,
    "The first json property value was incorrect."
  );

  // Open the response payload section, it should hide the json section
  wait = waitForDOM(document, "#response-panel .CodeMirror-code");
  const header = document.querySelector(
    "#response-panel .raw-data-toggle-input .devtools-checkbox-toggle"
  );
  clickElement(header, monitor);
  await wait;

  is(
    tabpanel.querySelector(".response-error-header") === null,
    true,
    "The response error header doesn't have the intended visibility."
  );
  const jsonView = tabpanel.querySelector(".data-label") || {};
  is(
    jsonView.textContent === L10N.getStr("jsonScopeName"),
    true,
    "The response json view has the intended visibility."
  );
  is(
    tabpanel.querySelector(".raw-data-toggle-input .devtools-checkbox-toggle")
      .checked,
    true,
    "The raw response toggle should be on."
  );
  is(
    tabpanel.querySelector(".CodeMirror-code") === null,
    false,
    "The response editor has the intended visibility."
  );
  is(
    tabpanel.querySelector(".response-image-box") === null,
    true,
    "The response image box doesn't have the intended visibility."
  );
  is(
    tabpanel.querySelectorAll(".empty-notice").length,
    0,
    "The empty notice should not be displayed in this tabpanel."
  );

  await teardown(monitor);
});
