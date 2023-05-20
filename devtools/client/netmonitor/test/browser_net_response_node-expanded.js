/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the node that was expanded is still expanded when we are filtering
 * in the Response Panel.
 */

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(JSON_LONG_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 1);

  info("selecting first request");
  const firstRequestItem = document.querySelectorAll(".request-list-item")[0];
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequestItem);

  info("switching to response panel");
  const waitForRespPanel = waitForDOM(
    document,
    "#response-panel .properties-view"
  );
  const respPanelButton = document.querySelector("#response-tab");
  respPanelButton.click();
  await waitForRespPanel;

  const firstRow = document.querySelector(
    "#response-panel tr.treeRow.objectRow"
  );
  const waitOpenNode = waitForDOM(document, "tr#\\/0\\/greeting");
  const toggleButton = firstRow.querySelector("td span.treeIcon");

  toggleButton.click();
  await waitOpenNode;

  is(firstRow.classList.contains("opened"), true, "the node is open");

  document.querySelector("#response-panel .devtools-filterinput").focus();
  EventUtils.sendString("greeting");

  // Wait till there are 2048 resources rendered in the results.
  await waitForDOMIfNeeded(document, "#response-panel tr.treeRow", 2048);

  is(firstRow.classList.contains("opened"), true, "the node remains open");

  await teardown(monitor);
});
