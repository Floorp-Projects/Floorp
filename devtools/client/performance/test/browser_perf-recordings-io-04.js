/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Tests if the performance tool can import profiler data from the
 * original profiler tool (Performance Recording v1, and Profiler data v2) and the correct views and graphs are loaded.
 */
var TICKS_DATA = (function () {
  let ticks = [];
  for (let i = 0; i < 100; i++) {
    ticks.push(i * 10);
  }
  return ticks;
})();

var PROFILER_DATA = (function () {
  let data = {};
  let threads = data.threads = [];
  let thread = {};
  threads.push(thread);
  thread.name = "Content";
  thread.samples = [{
    time: 5,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "B" },
      { location: "C" }
    ]
  }, {
    time: 5 + 6,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "B" },
      { location: "D" }
    ]
  }, {
    time: 5 + 6 + 7,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "E" },
      { location: "F" }
    ]
  }, {
    time: 20,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "B" },
      { location: "C" },
      { location: "D" },
      { location: "E" },
      { location: "F" },
      { location: "G" }
    ]
  }];

  // Handled in other deflating tests
  thread.markers = [];

  let meta = data.meta = {};
  meta.version = 2;
  meta.interval = 1;
  meta.stackwalk = 0;
  meta.product = "Firefox";
  return data;
})();

var test = Task.async(function* () {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, PerformanceController, DetailsView, OverviewView, JsCallTreeView } = panel.panelWin;

  // Enable memory to test the memory-calltree and memory-flamegraph.
  Services.prefs.setBoolPref(ALLOCATIONS_PREF, true);

  // Create a structure from the data that mimics the old profiler's data.
  // Different name for `ticks`, different way of storing time,
  // and no memory, markers data.
  let oldProfilerData = {
    profilerData: { profile: PROFILER_DATA },
    ticksData: TICKS_DATA,
    recordingDuration: 10000,
    fileType: "Recorded Performance Data",
    version: 1
  };

  // Save recording as an old profiler data.
  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));
  yield asyncCopy(oldProfilerData, file);

  // Import recording.

  let calltreeRendered = once(OverviewView, EVENTS.UI_FRAMERATE_GRAPH_RENDERED);
  let fpsRendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  let imported = once(PerformanceController, EVENTS.RECORDING_IMPORTED);
  yield PerformanceController.importRecording("", file);

  yield imported;
  ok(true, "The original profiler data appears to have been successfully imported.");

  yield calltreeRendered;
  yield fpsRendered;
  ok(true, "The imported data was re-rendered.");

  // Ensure that only framerate and js calltree/flamegraph view are available
  is(isVisible($("#overview-pane")), true, "overview graph container still shown");
  is(isVisible($("#memory-overview")), false, "memory graph hidden");
  is(isVisible($("#markers-overview")), false, "markers overview graph hidden");
  is(isVisible($("#time-framerate")), true, "fps graph shown");
  is($("#select-waterfall-view").hidden, true, "waterfall button hidden");
  is($("#select-js-calltree-view").hidden, false, "jscalltree button shown");
  is($("#select-js-flamegraph-view").hidden, false, "jsflamegraph button shown");
  is($("#select-memory-calltree-view").hidden, true, "memorycalltree button hidden");
  is($("#select-memory-flamegraph-view").hidden, true, "memoryflamegraph button hidden");
  ok(DetailsView.isViewSelected(JsCallTreeView), "jscalltree view selected as its the only option");

  // Verify imported recording.

  let importedData = PerformanceController.getCurrentRecording().getAllData();
  let expected = Object.create({
    duration: 10000,
    markers: [].toSource(),
    frames: [].toSource(),
    memory: [].toSource(),
    ticks: TICKS_DATA.toSource(),
    profile: RecordingUtils.deflateProfile(JSON.parse(JSON.stringify(PROFILER_DATA))).toSource(),
    allocations: ({sites:[], timestamps:[], frames:[], sizes: []}).toSource(),
    withTicks: true,
    withMemory: false,
    sampleFrequency: void 0
  });

  for (let field in expected) {
    if (!!~["withTicks", "withMemory", "sampleFrequency"].indexOf(field)) {
      is(importedData.configuration[field], expected[field], `${field} successfully converted in legacy import.`);
    } else if (field === "profile") {
      is(importedData.profile.toSource(), expected.profile,
        "profiler data's samples successfully converted in legacy import.");
      is(importedData.profile.meta.version, 3, "Updated meta version to 3.");
    } else {
      let data = importedData[field];
      is(typeof data === "object" ? data.toSource() : data, expected[field],
        `${field} successfully converted in legacy import.`);
    }
  }

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
/* eslint-enable */
