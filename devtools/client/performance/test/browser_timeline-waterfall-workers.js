/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Tests if the sidebar is properly updated with worker markers.
 */

async function spawnTest() {
  let { panel } = await initPerformance(WORKER_URL);
  let { $$, $, PerformanceController } = panel.panelWin;

  await startRecording(panel);
  ok(true, "Recording has started.");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.performWork();
  });

  await waitUntil(() => {
    // Wait until we get the worker markers.
    let markers = PerformanceController.getCurrentRecording().getMarkers();
    if (!markers.some(m => m.name == "Worker") ||
        !markers.some(m => m.workerOperation == "serializeDataOffMainThread") ||
        !markers.some(m => m.workerOperation == "serializeDataOnMainThread") ||
        !markers.some(m => m.workerOperation == "deserializeDataOffMainThread") ||
        !markers.some(m => m.workerOperation == "deserializeDataOnMainThread")) {
      return false;
    }

    testWorkerMarkerData(markers.find(m => m.name == "Worker"));
    return true;
  });

  await stopRecording(panel);
  ok(true, "Recording has ended.");

  for (let node of $$(".waterfall-marker-name[value=Worker")) {
    testWorkerMarkerUI(node.parentNode.parentNode);
  }

  await teardown(panel);
  finish();
}

function testWorkerMarkerData(marker) {
  ok(true, "Found a worker marker.");

  ok("start" in marker,
    "The start time is specified in the worker marker.");
  ok("end" in marker,
    "The end time is specified in the worker marker.");

  ok("workerOperation" in marker,
    "The worker operation is specified in the worker marker.");

  ok("processType" in marker,
    "The process type is specified in the worker marker.");
  ok("isOffMainThread" in marker,
    "The thread origin is specified in the worker marker.");
}

function testWorkerMarkerUI(node) {
  is(node.className, "waterfall-tree-item",
    "The marker node has the correct class name.");
  ok(node.hasAttribute("otmt"),
    "The marker node specifies if it is off the main thread or not.");
}

/* eslint-enable */
