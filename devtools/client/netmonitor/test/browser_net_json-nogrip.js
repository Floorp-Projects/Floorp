/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSON responses with property 'type' are correctly rendered.
 * (Reps rendering JSON responses should use `noGrip=true`).
 */
add_task(async function () {
  const { tab, monitor } = await initNetMonitor(
    JSON_BASIC_URL + "?name=nogrip",
    { requestCount: 1 }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 1);

  const onResponsePanelReady = waitForDOM(
    document,
    "#response-panel .data-header"
  );

  const onPropsViewReady = waitForDOM(
    document,
    "#response-panel .properties-view",
    1
  );

  store.dispatch(Actions.toggleNetworkDetails());
  clickOnSidebarTab(document, "response");
  await Promise.all([onResponsePanelReady, onPropsViewReady]);

  const tabpanel = document.querySelector("#response-panel");
  const labels = tabpanel.querySelectorAll("tr .treeLabelCell .treeLabel");
  const values = tabpanel.querySelectorAll("tr .treeValueCell .objectBox");

  is(labels[0].textContent, "obj", "The first json property name is correct.");
  is(
    values[0].textContent,
    'Object { type: "string" }',
    "The first json property value is correct."
  );

  await teardown(monitor);
});
