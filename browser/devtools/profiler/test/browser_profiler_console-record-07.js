/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler can correctly handle sequential console recordings,
 * finished in reverse order, and the second call to `console.profileEnd`
 * doesn't have any argument.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, EVENTS, gFront, RecordingsListView, ProfileView } = panel.panelWin;

  yield consoleProfile(gFront, "1");
  yield consoleProfile(gFront, "2");

  let firstRecordingDisplayed = panel.panelWin.once(EVENTS.RECORDING_DISPLAYED);
  yield consoleProfileEnd(gFront, true, "1");
  yield firstRecordingDisplayed;
  ok(true, "The newly finished console recording was automatically displayed.");

  is(RecordingsListView.itemCount, 2,
    "There should be two recordings visible.");
  is(RecordingsListView.items[0], RecordingsListView.selectedItem,
    "The first recording item should be selected.");

  is(RecordingsListView.items[0].attachment.profilerData.profileLabel, "1",
    "The profile label for the first recording is correct.");
  ok(!RecordingsListView.items[0].isRecording,
    "The 'isRecording' flag for the first recording is correct.");

  is(RecordingsListView.items[1].attachment.profilerData.profileLabel, "2",
    "The profile label for the second recording is correct.");
  ok(RecordingsListView.items[1].isRecording,
    "The 'isRecording' flag for the second recording is correct.");

  let secondRecordingDisplayed = panel.panelWin.once(EVENTS.RECORDING_DISPLAYED);
  yield consoleProfileEnd(gFront);
  yield secondRecordingDisplayed;
  ok(true, "The newly finished console recording was automatically redisplayed.");

  is(RecordingsListView.itemCount, 2,
    "There should still be two recordings visible.");
  is(RecordingsListView.items[1], RecordingsListView.selectedItem,
    "The second recording item should now be selected.");

  is(RecordingsListView.items[0].attachment.profilerData.profileLabel, "1",
    "The profile label for the first recording is still correct.");
  ok(!RecordingsListView.items[0].isRecording,
    "The 'isRecording' flag for the first recording is still correct.");

  is(RecordingsListView.items[1].attachment.profilerData.profileLabel, "2",
    "The profile label for the second recording is still correct.");
  ok(!RecordingsListView.items[1].isRecording,
    "The 'isRecording' flag for the second recording is still correct.");

  yield teardown(panel);
  finish();
});

function* consoleProfile(front, label) {
  let notified = front.once("profile");
  console.profile(label);
  yield notified;
}

function* consoleProfileEnd(front, withLabel, labelValue) {
  let notified = front.once("profileEnd");
  if (!withLabel) {
    console.profileEnd();
  } else {
    console.profileEnd(labelValue);
  }
  yield notified;
}
