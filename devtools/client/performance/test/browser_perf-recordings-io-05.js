/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Test that when importing and no graphs rendered yet, we do not get a
 * `getMappedSelection` error.
 */

var test = async function () {
  var { target, panel, toolbox } = await initPerformance(SIMPLE_URL);
  var { EVENTS, PerformanceController, WaterfallView } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  // Save recording.

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

  let exported = once(PerformanceController, EVENTS.RECORDING_EXPORTED);
  await PerformanceController.exportRecording("", PerformanceController.getCurrentRecording(), file);

  await exported;
  ok(true, "The recording data appears to have been successfully saved.");

  // Clear and re-import.

  await PerformanceController.clearRecordings();

  let rendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  let imported = once(PerformanceController, EVENTS.RECORDING_IMPORTED);
  await PerformanceController.importRecording("", file);
  await imported;
  await rendered;

  ok(true, "No error was thrown.");

  await teardown(panel);
  finish();
};
/* eslint-enable */
