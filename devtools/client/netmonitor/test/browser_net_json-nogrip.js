/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSON responses with property 'type' are correctly rendered.
 * (Reps rendering JSON responses should use `noGrip=true`).
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(JSON_BASIC_URL + "?name=nogrip");
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 1);

  const onResponsePanelReady = waitForDOM(document, "#response-panel .CodeMirror-code");
  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#response-tab"));
  await onResponsePanelReady;

  const tabpanel = document.querySelector("#response-panel");
  const labels = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
  const values = tabpanel
    .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

  // Verify that an object is rendered: `obj: {…}`
  is(labels[0].textContent, "obj", "The first json property name is correct.");
  is(values[0].textContent, "{\u2026}", "The first json property value is correct.");

  await teardown(monitor);
});
