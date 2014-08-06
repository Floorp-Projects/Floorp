/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is able to save and load recordings.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { RecordingsListView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel, { waitForDisplay: true });

  // Verify original recording.

  is(RecordingsListView.itemCount, 1,
    "There should be one recording visible now.");

  let originalRecordingItem = RecordingsListView.getItemAtIndex(0);
  ok(originalRecordingItem,
    "A recording item was available for import.");

  let originalData = originalRecordingItem.attachment;
  ok(originalData,
    "The original item has recording data attached to it.");

  // Save recording.

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

  yield panel.panelWin.saveRecordingToFile(originalRecordingItem, file);
  ok(true, "The recording data appears to have been successfully saved.");

  // Import recording.

  yield panel.panelWin.loadRecordingFromFile(file);
  ok(true, "The recording data appears to have been successfully imported.");

  // Verify imported recording.

  is(RecordingsListView.itemCount, 2,
    "There should be two recordings visible now.");

  let importedRecordingItem = RecordingsListView.getItemAtIndex(1);
  ok(importedRecordingItem,
    "A recording item was imported.");

  let importedData = importedRecordingItem.attachment;
  ok(importedData,
    "The imported item has recording data attached to it.");

  ok(("fileType" in originalData) && ("fileType" in importedData),
    "The serialization process attached an identifier to the recording data.");
  ok(("version" in originalData) && ("version" in importedData),
    "The serialization process attached a version to the recording data.");

  is(importedData.toSource(), originalData.toSource(),
    "The impored data is identical to the original data.");

  yield teardown(panel);
  finish();
});
