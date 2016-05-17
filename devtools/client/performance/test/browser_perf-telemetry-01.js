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

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { PerformanceController } = panel.panelWin;

  let telemetry = PerformanceController._telemetry;
  let logs = telemetry.getLogs();
  let DURATION = "DEVTOOLS_PERFTOOLS_RECORDING_DURATION_MS";
  let COUNT = "DEVTOOLS_PERFTOOLS_RECORDING_COUNT";
  let CONSOLE_COUNT = "DEVTOOLS_PERFTOOLS_CONSOLE_RECORDING_COUNT";
  let FEATURES = "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED";

  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, false);

  yield startRecording(panel);
  yield stopRecording(panel);

  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, true);

  yield startRecording(panel);
  yield stopRecording(panel);

  is(logs[DURATION].length, 2, `There are two entries for ${DURATION}.`);
  ok(logs[DURATION].every(d => typeof d === "number"), `Every ${DURATION} entry is a number.`);
  is(logs[COUNT].length, 2, `There are two entries for ${COUNT}.`);
  is(logs[CONSOLE_COUNT], void 0, `There are no entries for ${CONSOLE_COUNT}.`);
  is(logs[FEATURES].length, 8, `There are two recordings worth of entries for ${FEATURES}.`);
  ok(logs[FEATURES].find(r => r[0] === "withMemory" && r[1] === true), "One feature entry for memory enabled.");
  ok(logs[FEATURES].find(r => r[0] === "withMemory" && r[1] === false), "One feature entry for memory disabled.");

  yield teardownToolboxAndRemoveTab(panel);
});
