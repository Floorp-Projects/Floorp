/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is correctly populated by new console recordings
 * after it is opened.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, EVENTS, gFront, RecordingsListView, ProfileView } = panel.panelWin;

  yield consoleProfile(gFront, "hello world");

  is(RecordingsListView.itemCount, 1,
    "There should be one recording visible.");
  ok(!$(".side-menu-widget-empty-text"),
    "There shouldn't be any empty text displayed in the recordings list.");

  is(ProfileView.tabCount, 0,
    "There shouldn't be any tabs visible yet.");
  is($("#profile-pane").selectedPanel, $("#recording-notice"),
    "There should be a recording notice displayed in the profile view.");

  is(RecordingsListView.items[0], RecordingsListView.selectedItem,
    "The first and only recording item should be selected.");

  is(RecordingsListView.items[0].attachment.profilerData.profileLabel, "hello world",
    "The profile label for the first recording is correct.");
  ok(RecordingsListView.items[0].isRecording,
    "The 'isRecording' flag for the first recording is correct.");

  let recordingDisplayed = panel.panelWin.once(EVENTS.RECORDING_DISPLAYED);
  yield consoleProfileEnd(gFront);
  yield recordingDisplayed;
  ok(true, "The newly finished console recording was automatically displayed.");

  is(RecordingsListView.itemCount, 1,
    "There should still be one recording visible.");
  ok(!$(".side-menu-widget-empty-text"),
    "There still shouldn't be any empty text displayed in the recordings list.");

  is(ProfileView.tabCount, 1,
    "There should still be one tab visible.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should still be displayed in the profile view.");

  is(RecordingsListView.items[0], RecordingsListView.selectedItem,
    "The first and only recording item should still be selected.");

  is(RecordingsListView.items[0].attachment.profilerData.profileLabel, "hello world",
    "The profile label for the first recording is still correct.");
  ok(!RecordingsListView.items[0].isRecording,
    "The 'isRecording' flag for the first recording is still correct.");

  yield teardown(panel);
  finish();
});

function* consoleProfile(front, label) {
  let notified = front.once("profile");
  console.profile(label);
  yield notified;
}

function* consoleProfileEnd(front) {
  let notified = front.once("profileEnd");
  console.profileEnd();
  yield notified;
}
