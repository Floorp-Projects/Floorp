/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler gracefully handles loading bogus files.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { RecordingsListView } = panel.panelWin;

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

  try {
    yield panel.panelWin.loadRecordingFromFile(file);
    ok(false, "The recording succeeded unexpectedly.");
  } catch (e) {
    is(e.message, "Could not read recording data file.");
    ok(true, "The recording was cancelled.");
  }

  is(RecordingsListView.itemCount, 0,
    "There should still be no recordings visible.");

  yield teardown(panel);
  finish();
});
