/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the performance telemetry module records events at appropriate times.
 * Specifically export/import.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  startTelemetry();

  let { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, PerformanceController } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

  let exported = once(PerformanceController, EVENTS.RECORDING_EXPORTED);
  await PerformanceController.exportRecording(PerformanceController.getCurrentRecording(),
    file);
  await exported;

  let imported = once(PerformanceController, EVENTS.RECORDING_IMPORTED);
  await PerformanceController.importRecording(file);
  await imported;

  checkResults();
  await teardownToolboxAndRemoveTab(panel);
});

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_PERFTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_PERFTOOLS_RECORDING_IMPORT_FLAG", "", [0, 1, 0], "array");
  checkTelemetry("DEVTOOLS_PERFTOOLS_RECORDING_EXPORT_FLAG", "", [0, 1, 0], "array");
}
