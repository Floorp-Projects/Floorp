/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler properly handles the built-in profiler module
 * suddenly stopping.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, gFront, EVENTS, RecordingsListView } = panel.panelWin;

  yield consoleProfile(gFront, "test");
  yield startRecording(panel);

  is(gFront.pendingConsoleRecordings.length, 1,
    "There should be one pending console recording.");
  is(gFront.finishedConsoleRecordings.length, 0,
    "There should be no finished console recordings.");
  is(RecordingsListView.itemCount, 2,
    "There should be two recordings visible.");
  is($("#profile-pane").selectedPanel, $("#recording-notice"),
    "There should be a recording notice displayed in the profile view.");

  let whenFrontendNotified = gFront.once("profiler-unexpectedly-stopped");
  let whenRecordingLost = panel.panelWin.once(EVENTS.RECORDING_LOST);
  nsIProfilerModule.StopProfiler();

  yield whenFrontendNotified;
  ok(true, "The frontend was notified about the profiler being stopped.");

  yield whenRecordingLost;
  ok(true, "The frontend reacted to the profiler being stopped.");

  is(gFront.pendingConsoleRecordings.length, 0,
    "There should be no pending console recordings.");
  is(gFront.finishedConsoleRecordings.length, 0,
    "There should still be no finished console recordings.");
  is(RecordingsListView.itemCount, 0,
    "There should be no recordings visible.");
  is($("#profile-pane").selectedPanel, $("#empty-notice"),
    "There should be an empty notice displayed in the profile view.");

  yield teardown(panel);
  finish();
});

function* consoleProfile(front, label) {
  let notified = front.once("profile");
  console.profile(label);
  yield notified;
}
