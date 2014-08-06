/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is able clear all recordings.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, gFront, EVENTS, RecordingsListView, ProfileView } = panel.panelWin;

  // Start and finish a manual recording.
  yield startRecording(panel);
  yield stopRecording(panel, { waitForDisplay: true });

  // Start and finish a console recording.
  yield consoleProfile(gFront, "test 1");
  yield consoleProfileEnd(gFront, { waitForDisplayOn: panel });

  // Start a new manual and console recording, but keep them in progress.
  yield startRecording(panel);
  yield consoleProfile(gFront, "test 2");

  is(RecordingsListView.itemCount, 4,
    "There should be four recordings visible now.");
  is(RecordingsListView.selectedIndex, 1,
    "The second recording should be selected.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should be displayed in the profile view.");

  let whenRecordingLost = panel.panelWin.once(EVENTS.RECORDING_LOST);
  EventUtils.synthesizeMouseAtCenter($("#clear-button"), {}, panel.panelWin);
  yield whenRecordingLost;

  yield teardown(panel);
  finish();
});

function* consoleProfile(front, label) {
  let notified = front.once("profile");
  console.profile(label);
  yield notified;
}

function* consoleProfileEnd(front, { waitForDisplayOn: panel }) {
  let notified = front.once("profileEnd");
  let displayed = panel.panelWin.once(panel.panelWin.EVENTS.RECORDING_DISPLAYED);
  console.profileEnd();
  yield promise.all([notified, displayed]);
}
