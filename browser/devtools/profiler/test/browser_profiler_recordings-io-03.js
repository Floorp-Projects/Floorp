/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler gracefully handles loading files that are JSON,
 * but don't contain the appropriate recording data.
 */

let { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
let { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { RecordingsListView } = panel.panelWin;

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));
  yield asyncCopy({ bogus: "data" }, file);

  try {
    yield panel.panelWin.loadRecordingFromFile(file);
    ok(false, "The recording succeeded unexpectedly.");
  } catch (e) {
    is(e.message, "Unrecognized recording data file.");
    ok(true, "The recording was cancelled.");
  }

  is(RecordingsListView.itemCount, 0,
    "There should still be no recordings visible.");

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
  let deferred = promise.defer();

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
