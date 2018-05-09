/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSON responses with property 'type' are correctly rendered.
 * (Reps rendering JSON responses should use `noGrip=true`).
 */
add_task(async function() {
  let { tab, monitor } = await initNetMonitor(JSON_BASIC_URL + "?name=nogrip");
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 1);

  let onResponsePanelReady = waitForDOM(document, "#response-panel .CodeMirror-code");
  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#response-tab"));
  await onResponsePanelReady;

  let tabpanel = document.querySelector("#response-panel");
  let labels = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
  let values = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

  // Verify that an object is rendered: `obj: {…}`
  is(labels[0].textContent, "obj", "The first json property name is correct.");
  is(values[0].textContent, "{\u2026}", "The first json property value is correct.");

  await teardown(monitor);
});
