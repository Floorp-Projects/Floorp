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

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, PerformanceController } = panel.panelWin;

  let telemetry = PerformanceController._telemetry;
  let logs = telemetry.getLogs();
  let EXPORTED = "DEVTOOLS_PERFTOOLS_RECORDING_EXPORT_FLAG";
  let IMPORTED = "DEVTOOLS_PERFTOOLS_RECORDING_IMPORT_FLAG";

  yield startRecording(panel);
  yield stopRecording(panel);

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

  let exported = once(PerformanceController, EVENTS.RECORDING_EXPORTED);
  yield PerformanceController.exportRecording("", PerformanceController.getCurrentRecording(), file);
  yield exported;

  ok(logs[EXPORTED], `A telemetry entry for ${EXPORTED} exists after exporting.`);

  let imported = once(PerformanceController, EVENTS.RECORDING_IMPORTED);
  yield PerformanceController.importRecording(null, file);
  yield imported;

  ok(logs[IMPORTED], `A telemetry entry for ${IMPORTED} exists after importing.`);

  yield teardownToolboxAndRemoveTab(panel);
});
