/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSON responses and requests with XSSI protection sequences
 * are handled correctly.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(JSON_XSSI_PROTECTION_URL, {
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
  info("Opening response panel");
  clickOnSidebarTab(document, "response");

  await Promise.all([wait, waitForPropsView]);

  const tabpanel = document.querySelector("#response-panel");
  is(
    tabpanel.querySelectorAll(".treeRow").length,
    1,
    "There should be 1 json property displayed in the response."
  );

  const labels = tabpanel.querySelectorAll("tr .treeLabelCell .treeLabel");
  const values = tabpanel.querySelectorAll("tr .treeValueCell .objectBox");
  info("Checking content of displayed json response");
  is(labels[0].textContent, "greeting", "The first key should be correct");
  is(
    values[0].textContent,
    `"Hello good XSSI protection"`,
    "The first property should be correct"
  );

  info("switching to raw view");
  wait = waitForDOM(document, "#response-panel .CodeMirror-code");
  const rawResponseToggle = tabpanel.querySelector("#raw-response-checkbox");
  clickElement(rawResponseToggle, monitor);
  await wait;

  info("making sure XSSI protection is in raw view");
  const codeLines = document.querySelector("#response-panel .CodeMirror-code");
  const firstLine = codeLines.firstChild;
  const firstLineText = firstLine.querySelector("pre.CodeMirror-line span");
  is(
    firstLineText.textContent,
    ")]}'",
    "XSSI protection sequence should be visibly in raw view"
  );

  await teardown(monitor);
});
