/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Tests if the sidebar is properly updated with worker markers.
 */

function* spawnTest() {
  let { panel } = yield initPerformance(WORKER_URL);
  let { $$, $, PerformanceController } = panel.panelWin;

  loadFrameScripts();

  yield startRecording(panel);
  ok(true, "Recording has started.");

  evalInDebuggee("performWork()");

  yield waitUntil(() => {
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

  yield stopRecording(panel);
  ok(true, "Recording has ended.");

  for (let node of $$(".waterfall-marker-name[value=Worker")) {
    testWorkerMarkerUI(node.parentNode.parentNode);
  }

  yield teardown(panel);
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

/**
 * Takes a string `script` and evaluates it directly in the content
 * in potentially a different process.
 */
function evalInDebuggee(script) {
  let { generateUUID } = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  let deferred = Promise.defer();

  if (!mm) {
    throw new Error("`loadFrameScripts()` must be called when using MessageManager.");
  }

  let id = generateUUID().toString();
  mm.sendAsyncMessage("devtools:test:eval", { script: script, id: id });
  mm.addMessageListener("devtools:test:eval:response", handler);

  function handler({ data }) {
    if (id !== data.id) {
      return;
    }

    mm.removeMessageListener("devtools:test:eval:response", handler);
    deferred.resolve(data.value);
  }

  return deferred.promise;
}
/* eslint-enable */
