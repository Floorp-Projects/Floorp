/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the performance tool can import profiler data from the
 * original profiler tool and the correct views and graphs are loaded.
 */

let test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, PerformanceController, DetailsView, OverviewView, JsCallTreeView } = panel.panelWin;

  // Enable memory to test the memory-calltree and memory-flamegraph.
  Services.prefs.setBoolPref(MEMORY_PREF, true);

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

  let calltreeRendered = once(OverviewView, EVENTS.FRAMERATE_GRAPH_RENDERED);
  let fpsRendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  let imported = once(PerformanceController, EVENTS.RECORDING_IMPORTED);
  yield PerformanceController.importRecording("", file);

  yield imported;
  ok(true, "The original profiler data appears to have been successfully imported.");

  yield calltreeRendered;
  yield fpsRendered;
  ok(true, "The imported data was re-rendered.");

  // Ensure that only framerate and js calltree/flamegraph view are available
  is($("#overview-pane").hidden, false, "overview graph container still shown");
  is($("#memory-overview").hidden, true, "memory graph hidden");
  is($("#markers-overview").hidden, true, "markers overview graph hidden");
  is($("#time-framerate").hidden, false, "fps graph shown");
  is($("#select-waterfall-view").hidden, true, "waterfall button hidden");
  is($("#select-js-calltree-view").hidden, false, "jscalltree button shown");
  is($("#select-js-flamegraph-view").hidden, false, "jsflamegraph button shown");
  is($("#select-memory-calltree-view").hidden, true, "memorycalltree button hidden");
  is($("#select-memory-flamegraph-view").hidden, true, "memoryflamegraph button hidden");
  ok(DetailsView.isViewSelected(JsCallTreeView), "jscalltree view selected as its the only option");

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
  is(importedData.allocations.toSource(), ({sites:[], timestamps:[], frames:[], counts:[]}).toSource(),
    "The imported legacy data was successfully converted for the current tool (7).");
  is(importedData.profile.toSource(), data.profile.toSource(),
    "The imported legacy data was successfully converted for the current tool (8).");
  is(importedData.configuration.withTicks, true,
    "The imported legacy data was successfully converted for the current tool (9).");
  is(importedData.configuration.withMemory, false,
    "The imported legacy data was successfully converted for the current tool (10).");
  is(importedData.configuration.sampleFrequency, void 0,
    "The imported legacy data was successfully converted for the current tool (11).");

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
