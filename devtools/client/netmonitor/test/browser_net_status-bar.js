/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test whether the StatusBar properly renders expected labels.
 */
add_task(async () => {
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await SpecialPowers.pushPrefEnv({ "set": [["privacy.reduceTimerPrecision", false]]});

  const requestsDone = waitForAllRequestsFinished(monitor);
  const markersDone = waitForTimelineMarkers(monitor);
  tab.linkedBrowser.reload();
  await Promise.all([requestsDone, markersDone]);

  const statusBar = document.querySelector(".devtools-toolbar-bottom");
  const requestCount = statusBar.querySelector(".requests-list-network-summary-count");
  const size = statusBar.querySelector(".requests-list-network-summary-transfer");
  const onContentLoad = statusBar.querySelector(".dom-content-loaded");
  const onLoad = statusBar.querySelector(".load");

  // All expected labels should be there
  ok(requestCount, "There must be request count label");
  ok(size, "There must be size label");
  ok(onContentLoad, "There must be DOMContentLoaded label");
  ok(onLoad, "There must be load label");

  // The content should not be empty. The UI update can also be async,
  // so use waitUntil.
  await waitUntil(() => requestCount.textContent);
  ok(true, "There must be request count label text");

  await waitUntil(() => size.textContent);
  ok(true, "There must be size label text");

  await waitUntil(() => onContentLoad.textContent);
  ok(true, "There must be DOMContentLoaded label text");

  await waitUntil(() => onLoad.textContent);
  ok(true, "There must be load label text");

  return teardown(monitor);
});
