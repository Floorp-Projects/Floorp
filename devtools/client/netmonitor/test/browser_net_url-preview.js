/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the url preview expanded state is persisted across requests selections.
 */

add_task(async function() {
  const { monitor, tab } = await initNetMonitor(PARAMS_URL, {
    requestCount: 1,
  });

  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 3);

  let wait = waitForDOM(document, "#headers-panel .url-preview", 1);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await wait;

  // Expand preview
  await toggleUrlPreview(true, document, monitor);

  // Select the second request
  wait = waitForDOM(document, "#headers-panel .url-preview", 1);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[1]
  );
  await wait;

  // Test that the url is still expanded
  const noOfVisibleRowsAfterExpand = document.querySelectorAll(
    "#headers-panel .url-preview tr.treeRow"
  ).length;
  ok(
    noOfVisibleRowsAfterExpand > 1,
    "The url preview should still be expanded."
  );

  // Collapse preview
  await toggleUrlPreview(false, document, monitor);

  // Select the third request
  wait = waitForDOM(document, "#headers-panel .url-preview", 1);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[2]
  );
  await wait;

  // Test that the url is still collapsed
  const noOfVisibleRowsAfterCollapse = document.querySelectorAll(
    "#headers-panel .url-preview tr.treeRow"
  ).length;
  ok(
    noOfVisibleRowsAfterCollapse == 1,
    "The url preview should still be collapsed."
  );

  return teardown(monitor);
});

async function toggleUrlPreview(shouldExpand, document, monitor) {
  const wait = waitUntil(() => {
    const rowSize = document.querySelectorAll(
      "#headers-panel .url-preview tr.treeRow"
    ).length;
    return shouldExpand ? rowSize > 1 : rowSize == 1;
  });

  clickElement(
    document.querySelector(
      "#headers-panel .url-preview tr:first-child span.treeIcon.theme-twisty"
    ),
    monitor
  );
  return wait;
}
