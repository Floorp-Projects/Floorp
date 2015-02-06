/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the performance tool is able to save and load recordings.
 */

let test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, DetailsView, DetailsSubview } = panel.panelWin;

  // Enable memory to test the memory-calltree and memory-flamegraph.
  Services.prefs.setBoolPref(MEMORY_PREF, true);

  // Cycle through all the views to initialize them, otherwise we can't use
  // `waitForWidgetsRendered`. The waterfall is shown by default, but all the
  // other views are created lazily, so won't emit any events.
  yield DetailsView.selectView("js-calltree");
  yield DetailsView.selectView("js-flamegraph");
  yield DetailsView.selectView("memory-calltree");
  yield DetailsView.selectView("memory-flamegraph");

  // Need to allow widgets to be updated while hidden, otherwise we can't use
  // `waitForWidgetsRendered`.
  DetailsSubview.canUpdateWhileHidden = true;

  yield startRecording(panel);
  yield stopRecording(panel);

  // Verify original recording.

  let originalData = PerformanceController.getCurrentRecording().getAllData();
  ok(originalData, "The original recording is not empty.");

  // Save recording.

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

  let exported = once(PerformanceController, EVENTS.RECORDING_EXPORTED);
  yield PerformanceController.exportRecording("", PerformanceController.getCurrentRecording(), file);

  yield exported;
  ok(true, "The recording data appears to have been successfully saved.");

  // Import recording.

  let rerendered = waitForWidgetsRendered(panel);
  let imported = once(PerformanceController, EVENTS.RECORDING_IMPORTED);
  yield PerformanceController.importRecording("", file);

  yield imported;
  ok(true, "The recording data appears to have been successfully imported.");

  yield rerendered;
  ok(true, "The imported data was re-rendered.");

  // Verify imported recording.

  let importedData = PerformanceController.getCurrentRecording().getAllData();

  is(importedData.label, originalData.label,
    "The impored data is identical to the original data (1).");
  is(importedData.duration, originalData.duration,
    "The impored data is identical to the original data (2).");
  is(importedData.markers.toSource(), originalData.markers.toSource(),
    "The impored data is identical to the original data (3).");
  is(importedData.memory.toSource(), originalData.memory.toSource(),
    "The impored data is identical to the original data (4).");
  is(importedData.ticks.toSource(), originalData.ticks.toSource(),
    "The impored data is identical to the original data (5).");
  is(importedData.allocations.toSource(), originalData.allocations.toSource(),
    "The impored data is identical to the original data (6).");
  is(importedData.profile.toSource(), originalData.profile.toSource(),
    "The impored data is identical to the original data (7).");

  yield teardown(panel);
  finish();
});
