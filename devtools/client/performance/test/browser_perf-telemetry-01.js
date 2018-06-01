/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the performance telemetry module records events at appropriate times.
 * Specificaly the state about a recording process.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_MEMORY_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");

add_task(async function() {
  startTelemetry();

  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, false);

  await startRecording(panel);
  await stopRecording(panel);

  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, true);

  await startRecording(panel);
  await stopRecording(panel);

  checkResults();

  await teardownToolboxAndRemoveTab(panel);
});

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_PERFTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_PERFTOOLS_RECORDING_COUNT", "", [2, 0, 0], "array");
  checkTelemetry("DEVTOOLS_PERFTOOLS_RECORDING_DURATION_MS", "", null, "hasentries");
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED", "withMarkers", [0, 2, 0], "array");
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED", "withMemory", [1, 1, 0], "array");
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED", "withAllocations", [2, 0, 0], "array");
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED", "withTicks", [0, 2, 0], "array");
}
