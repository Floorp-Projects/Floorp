/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that `enable-memory` toggles the visibility of the memory graph,
 * as well as enabling memory data on the PerformanceFront.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_MEMORY_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { isVisible } = require("devtools/client/performance/test/helpers/dom-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { $, PerformanceController } = panel.panelWin;

  // Disable memory to test.
  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, false);

  await startRecording(panel);
  await stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withMemory, false,
    "PerformanceFront started without memory recording.");
  is(PerformanceController.getCurrentRecording().getConfiguration().withAllocations,
    false, "PerformanceFront started without allocations recording.");
  ok(!isVisible($("#memory-overview")),
    "The memory graph is hidden when memory disabled.");

  // Re-enable memory.
  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, true);

  is(PerformanceController.getCurrentRecording().getConfiguration().withMemory, false,
    "PerformanceFront still marked without memory recording.");
  is(PerformanceController.getCurrentRecording().getConfiguration().withAllocations,
    false, "PerformanceFront still marked without allocations recording.");
  ok(!isVisible($("#memory-overview")), "memory graph is still hidden after enabling " +
                                        "if recording did not start recording memory");

  await startRecording(panel);
  await stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withMemory, true,
    "PerformanceFront started with memory recording.");
  is(PerformanceController.getCurrentRecording().getConfiguration().withAllocations,
    false, "PerformanceFront did not record with allocations.");
  ok(isVisible($("#memory-overview")),
    "The memory graph is not hidden when memory enabled before recording.");

  await teardownToolboxAndRemoveTab(panel);
});
