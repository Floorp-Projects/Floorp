/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler removes all pending recordings in case of a
 * sudden deactivation, even the ones started via `console.profile`.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, EVENTS, gFront, RecordingsListView } = panel.panelWin;

  is(RecordingsListView.itemCount, 0,
    "There should be no recordings visible yet.");
  is($("#profile-pane").selectedPanel, $("#empty-notice"),
    "There should be an empty notice displayed in the profile view.");

  yield startRecording(panel);
  yield consoleProfile(gFront, "test");

  is(RecordingsListView.itemCount, 2,
    "There should be two recordings visible now.");
  is(RecordingsListView.selectedIndex, 0,
    "The first recording should be selected in the list.");
  is($("#profile-pane").selectedPanel, $("#recording-notice"),
    "There should be a recording notice displayed in the profile view.");

  let whenLost = panel.panelWin.once(EVENTS.RECORDING_LOST);

  // Forcibly stop the built-in profiler module.
  nsIProfilerModule.StopProfiler();

  yield whenLost;
  ok(true, "Recording was cancelled in the frontend.");

  is(RecordingsListView.itemCount, 0,
    "There should be no recordings visible now.");
  is(RecordingsListView.selectedIndex, -1,
    "There shouldn't be any recording selected in the list.");
  is($("#profile-pane").selectedPanel, $("#empty-notice"),
    "There should be an empty notice displayed in the profile view again.");

  ok(!$("#record-button").hasAttribute("checked"),
    "The record button was unchecked.");

  yield teardown(panel);
  finish();
});

function* consoleProfile(front, label) {
  let notified = front.once("profile");
  console.profile(label);
  yield notified;
}
