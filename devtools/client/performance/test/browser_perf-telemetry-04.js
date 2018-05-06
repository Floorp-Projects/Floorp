/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the performance telemetry module records events at appropriate times.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInTab, initConsoleInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { waitForRecordingStartedEvents, waitForRecordingStoppedEvents } = require("devtools/client/performance/test/helpers/actions");

add_task(async function() {
  startTelemetry();

  let { target, console } = await initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { panel } = await initPerformanceInTab({ tab: target.tab });

  let started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true
  });
  await console.profile("rust");
  await started;

  let stopped = waitForRecordingStoppedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true
  });
  await console.profileEnd("rust");
  await stopped;

  checkResults();
  await teardownToolboxAndRemoveTab(panel);
});

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_PERFTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_PERFTOOLS_CONSOLE_RECORDING_COUNT", "", [1, 0, 0], "array");
  checkTelemetry("DEVTOOLS_PERFTOOLS_RECORDING_DURATION_MS", "", null, "hasentries");
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED", "withMarkers", [0, 1, 0], "array");
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED", "withMemory", [1, 0, 0], "array");
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED", "withAllocations", [1, 0, 0], "array");
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED", "withTicks", [0, 1, 0], "array");
}
