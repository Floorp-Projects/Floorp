/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is correctly populated by sequential console recordings
 * with the same label, after it is opened.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, EVENTS, gFront, RecordingsListView, ProfileView } = panel.panelWin;

  yield consoleProfile(gFront, "hello world");
  let firstRecordingDisplayed = panel.panelWin.once(EVENTS.RECORDING_DISPLAYED);
  yield consoleProfileEnd(gFront);
  yield firstRecordingDisplayed;
  ok(true, "The newly finished console recording was automatically displayed.");

  yield consoleProfile(gFront, "hello world");
  let secondRecordingDisplayed = panel.panelWin.once(EVENTS.RECORDING_DISPLAYED);
  yield consoleProfileEnd(gFront);
  yield secondRecordingDisplayed;
  ok(true, "The newly finished console recording was automatically redisplayed.");

  is(RecordingsListView.itemCount, 1,
    "There should be just one recording visible.");
  ok(!$(".side-menu-widget-empty-text"),
    "There shouldn't be any empty text displayed in the recordings list.");

  is(ProfileView.tabCount, 1,
    "There should be one tab visible.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should be displayed in the profile view.");

  is(RecordingsListView.items[0], RecordingsListView.selectedItem,
    "The first and only recording item should be selected.");

  is(RecordingsListView.items[0].attachment.profilerData.profileLabel, "hello world",
    "The profile label for the first recording is correct.");
  ok(!RecordingsListView.items[0].isRecording,
    "The 'isRecording' flag for the first recording is correct.");

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
