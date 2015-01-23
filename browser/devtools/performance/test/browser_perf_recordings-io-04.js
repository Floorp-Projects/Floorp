/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the performance tool can import profiler data from the
 * original profiler tool.
 */

let test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  // Get data from the current profiler
  let data = PerformanceController.getCurrentRecording().getAllData();

  // Create a structure from the data that mimics the old profiler's data.
  // Different name for `ticks`, different way of storing time,
  // and no memory, markers data.
  let oldProfilerData = {
    profilerData: { profile: data.profile },
    ticksData: data.ticks,
    recordingDuration: data.duration,
    fileType: "Recorded Performance Data",
    version: 1
  };

  // Save recording as an old profiler data.
  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));
  yield asyncCopy(oldProfilerData, file);

  // Import recording.

  let rerendered = waitForWidgetsRendered(panel);
  let imported = once(PerformanceController, EVENTS.RECORDING_IMPORTED);
  yield PerformanceController.importRecording("", file);

  yield imported;
  ok(true, "The original profiler data appears to have been successfully imported.");

  yield rerendered;
  ok(true, "The imported data was re-rendered.");

  // Verify imported recording.

  let importedData = PerformanceController.getCurrentRecording().getAllData();

  is(importedData.label, data.label,
    "The imported legacy data was successfully converted for the current tool (1).");
  is(importedData.duration, data.duration,
    "The imported legacy data was successfully converted for the current tool (2).");
  is(importedData.markers.toSource(), [].toSource(),
    "The imported legacy data was successfully converted for the current tool (3).");
  is(importedData.frames.toSource(), [].toSource(),
    "The imported legacy data was successfully converted for the current tool (4).");
  is(importedData.memory.toSource(), [].toSource(),
    "The imported legacy data was successfully converted for the current tool (5).");
  is(importedData.ticks.toSource(), data.ticks.toSource(),
    "The imported legacy data was successfully converted for the current tool (6).");
  is(importedData.profile.toSource(), data.profile.toSource(),
    "The imported legacy data was successfully converted for the current tool (7).");

  yield teardown(panel);
  finish();
});

function getUnicodeConverter() {
  let className = "@mozilla.org/intl/scriptableunicodeconverter";
  let converter = Cc[className].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  return converter;
}

function asyncCopy(data, file) {
  let deferred = Promise.defer();

  let string = JSON.stringify(data);
  let inputStream = getUnicodeConverter().convertToInputStream(string);
  let outputStream = FileUtils.openSafeFileOutputStream(file);

  NetUtil.asyncCopy(inputStream, outputStream, status => {
    if (!Components.isSuccessCode(status)) {
      deferred.reject(new Error("Could not save data to file."));
    }
    deferred.resolve();
  });

  return deferred.promise;
}
