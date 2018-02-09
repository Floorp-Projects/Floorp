/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test whether the StatusBar properly renders expected labels.
 */
add_task(async () => {
  let { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await SpecialPowers.pushPrefEnv({ "set": [["privacy.reduceTimerPrecision", false]]});

  let requestsDone = waitForAllRequestsFinished(monitor);
  let markersDone = waitForTimelineMarkers(monitor);
  tab.linkedBrowser.reload();
  await Promise.all([requestsDone, markersDone]);

  let statusBar = document.querySelector(".devtools-toolbar-bottom");
  let requestCount = statusBar.querySelector(".requests-list-network-summary-count");
  let size = statusBar.querySelector(".requests-list-network-summary-transfer");
  let onContentLoad = statusBar.querySelector(".dom-content-loaded");
  let onLoad = statusBar.querySelector(".load");

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
