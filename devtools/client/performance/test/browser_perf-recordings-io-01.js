/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the performance tool is able to save and load recordings.
 */

var test = Task.async(function*() {
  var { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  var { $, EVENTS, PerformanceController, PerformanceView, DetailsView, DetailsSubview } = panel.panelWin;

  // Enable allocations to test the memory-calltree and memory-flamegraph.
  Services.prefs.setBoolPref(ALLOCATIONS_PREF, true);
  Services.prefs.setBoolPref(MEMORY_PREF, true);
  Services.prefs.setBoolPref(FRAMERATE_PREF, true);

  // Need to allow widgets to be updated while hidden, otherwise we can't use
  // `waitForWidgetsRendered`.
  DetailsSubview.canUpdateWhileHidden = true;

  yield startRecording(panel);
  yield stopRecording(panel);

  // Cycle through all the views to initialize them, otherwise we can't use
  // `waitForWidgetsRendered`. The waterfall is shown by default, but all the
  // other views are created lazily, so won't emit any events.
  yield DetailsView.selectView("js-calltree");
  yield DetailsView.selectView("js-flamegraph");
  yield DetailsView.selectView("memory-calltree");
  yield DetailsView.selectView("memory-flamegraph");

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

 //  Check if the imported file name has tmpprofile in it as the file
 //  names also has different suffix to avoid conflict

  let displayedName = $(".recording-item-title").getAttribute("value");
  ok(/^tmpprofile/.test(displayedName), "File has expected display name after import");
  ok(!/\.json$/.test(displayedName), "Display name does not have .json in it");

  // Import recording.

  let rerendered = waitForWidgetsRendered(panel);
  let imported = once(PerformanceController, EVENTS.RECORDING_IMPORTED);
  PerformanceView.emit(EVENTS.UI_IMPORT_RECORDING, file);

  yield imported;
  ok(true, "The recording data appears to have been successfully imported.");

  yield rerendered;
  ok(true, "The imported data was re-rendered.");

  // Verify imported recording.

  let importedData = PerformanceController.getCurrentRecording().getAllData();

  ok(/^tmpprofile/.test(importedData.label),
    "The imported data label is identical to the filename without its extension.");
  is(importedData.duration, originalData.duration,
    "The imported data is identical to the original data (1).");
  is(importedData.markers.toSource(), originalData.markers.toSource(),
    "The imported data is identical to the original data (2).");
  is(importedData.memory.toSource(), originalData.memory.toSource(),
    "The imported data is identical to the original data (3).");
  is(importedData.ticks.toSource(), originalData.ticks.toSource(),
    "The imported data is identical to the original data (4).");
  is(importedData.allocations.toSource(), originalData.allocations.toSource(),
    "The imported data is identical to the original data (5).");
  is(importedData.profile.toSource(), originalData.profile.toSource(),
    "The imported data is identical to the original data (6).");
  is(importedData.configuration.withTicks, originalData.configuration.withTicks,
    "The imported data is identical to the original data (7).");
  is(importedData.configuration.withMemory, originalData.configuration.withMemory,
    "The imported data is identical to the original data (8).");

  yield teardown(panel);
  finish();
});
