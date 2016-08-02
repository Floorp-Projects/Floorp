/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Tests if the performance tool can import profiler data when Profiler is v2
 * and requires deflating, and has an extra thread that's a string. Not sure
 * what causes this.
 */
var STRINGED_THREAD = (function () {
  let thread = {};

  thread.libs = [{
    start: 123,
    end: 456,
    offset: 0,
    name: "",
    breakpadId: ""
  }];
  thread.meta = { version: 2, interval: 1, stackwalk: 0, processType: 1, startTime: 0 };
  thread.threads = [{
    name: "Plugin",
    tid: 4197,
    samples: [],
    markers: [],
  }];

  return JSON.stringify(thread);
})();

var PROFILER_DATA = (function () {
  let data = {};
  let threads = data.threads = [];
  let thread = {};
  threads.push(thread);
  threads.push(STRINGED_THREAD);
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
  let { $, EVENTS, PerformanceController, DetailsView, JsCallTreeView } = panel.panelWin;

  let profilerData = {
    profile: PROFILER_DATA,
    duration: 10000,
    configuration: {},
    fileType: "Recorded Performance Data",
    version: 2
  };

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));
  yield asyncCopy(profilerData, file);

  // Import recording.

  let calltreeRendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  let imported = once(PerformanceController, EVENTS.RECORDING_IMPORTED);
  yield PerformanceController.importRecording("", file);

  yield imported;
  ok(true, "The profiler data appears to have been successfully imported.");

  yield calltreeRendered;
  ok(true, "The imported data was re-rendered.");

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
