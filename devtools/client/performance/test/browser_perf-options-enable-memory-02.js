/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that toggling `enable-memory` during a recording doesn't change that
 * recording's state and does not break.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_MEMORY_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { PerformanceController } = panel.panelWin;

  // Test starting without memory, and stopping with it.
  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, false);
  await startRecording(panel);

  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, true);
  await stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withMemory, false,
    "The recording finished without tracking memory.");
  is(PerformanceController.getCurrentRecording().getConfiguration().withAllocations,
    false,
    "The recording finished without tracking allocations.");

  // Test starting with memory, and stopping without it.
  await startRecording(panel);

  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, false);
  await stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withMemory, true,
    "The recording finished with tracking memory.");
  is(PerformanceController.getCurrentRecording().getConfiguration().withAllocations,
    false,
    "The recording still is not recording allocations.");

  await teardownToolboxAndRemoveTab(panel);
});
