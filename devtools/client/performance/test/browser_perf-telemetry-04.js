/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the performance telemetry module records events at appropriate times.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInTab, initConsoleInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { waitForRecordingStartedEvents, waitForRecordingStoppedEvents } = require("devtools/client/performance/test/helpers/actions");

add_task(function* () {
  let { target, console } = yield initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { panel } = yield initPerformanceInTab({ tab: target.tab });
  let { PerformanceController } = panel.panelWin;

  let telemetry = PerformanceController._telemetry;
  let logs = telemetry.getLogs();
  let DURATION = "DEVTOOLS_PERFTOOLS_RECORDING_DURATION_MS";
  let CONSOLE_COUNT = "DEVTOOLS_PERFTOOLS_CONSOLE_RECORDING_COUNT";
  let FEATURES = "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED";

  let started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true
  });
  yield console.profile("rust");
  yield started;

  let stopped = waitForRecordingStoppedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true
  });
  yield console.profileEnd("rust");
  yield stopped;

  is(logs[DURATION].length, 1, `There is one entry for ${DURATION}.`);
  ok(logs[DURATION].every(d => typeof d === "number"),
     `Every ${DURATION} entry is a number.`);
  is(logs[CONSOLE_COUNT].length, 1, `There is one entry for ${CONSOLE_COUNT}.`);
  is(logs[FEATURES].length, 4,
     `There is one recording worth of entries for ${FEATURES}.`);

  yield teardownToolboxAndRemoveTab(panel);
});
