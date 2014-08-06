/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is populated by in-progress console recordings, and
 * also console recordings that have finished before it was opened.
 */

let test = Task.async(function*() {
  let profilerConnected = waitForProfilerConnection();

  let [target, debuggee, networkPanel] = yield initFrontend(SIMPLE_URL, "netmonitor");
  let toolbox = networkPanel._toolbox;

  let SharedProfilerUtils = devtools.require("devtools/profiler/shared");
  let sharedProfilerConnection = SharedProfilerUtils.getProfilerConnection(toolbox);

  yield profilerConnected;
  yield consoleProfile(sharedProfilerConnection, 1);
  yield consoleProfile(sharedProfilerConnection, 2);
  yield consoleProfileEnd(sharedProfilerConnection);

  yield toolbox.selectTool("jsprofiler");
  let profilerPanel = toolbox.getCurrentPanel();
  let { $, EVENTS, RecordingsListView, ProfileView } = profilerPanel.panelWin;

  yield profilerPanel.panelWin.once(EVENTS.RECORDING_DISPLAYED);
  ok(true, "The already finished console recording was automatically displayed.");

  is(RecordingsListView.itemCount, 2,
    "There should be two recordings visible.");
  ok(!$(".side-menu-widget-empty-text"),
    "There shouldn't be any empty text displayed in the recordings list.");

  is(ProfileView.tabCount, 1,
    "There should be one tab visible.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should be displayed in the profile view.");

  is(RecordingsListView.items[0], RecordingsListView.selectedItem,
    "The first recording item should be automatically selected.");

  is(RecordingsListView.items[0].attachment.profilerData.profileLabel, "2",
    "The profile label for the first recording is correct.");
  ok(!RecordingsListView.items[0].isRecording,
    "The 'isRecording' flag for the first recording is correct.");

  is(RecordingsListView.items[1].attachment.profilerData.profileLabel, "1",
    "The profile label for the second recording is correct.");
  ok(RecordingsListView.items[1].isRecording,
    "The 'isRecording' flag for the second recording is correct.");

  let recordingDisplayed = profilerPanel.panelWin.once(EVENTS.RECORDING_DISPLAYED);
  yield consoleProfileEnd(sharedProfilerConnection);
  yield recordingDisplayed;
  ok(true, "The newly finished console recording was automatically displayed.");

  is(RecordingsListView.itemCount, 2,
    "There should still be two recordings visible.");
  ok(!$(".side-menu-widget-empty-text"),
    "There still shouldn't be any empty text displayed in the recordings list.");

  is(ProfileView.tabCount, 1,
    "There should still be one tab visible.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should still be displayed in the profile view.");

  is(RecordingsListView.items[1], RecordingsListView.selectedItem,
    "The second recording item should still be selected.");

  is(RecordingsListView.items[0].attachment.profilerData.profileLabel, "2",
    "The profile label for the first recording is still correct.");
  ok(!RecordingsListView.items[0].isRecording,
    "The 'isRecording' flag for the first recording is still correct.");

  is(RecordingsListView.items[1].attachment.profilerData.profileLabel, "1",
    "The profile label for the second recording is still correct.");
  ok(!RecordingsListView.items[1].isRecording,
    "The 'isRecording' flag for the second recording is still correct.");

  yield teardown(profilerPanel);
  finish();
});

function* consoleProfile(connection, label) {
  let notified = connection.once("profile");
  console.profile(label);
  yield notified;
}

function* consoleProfileEnd(connection) {
  let notified = connection.once("profileEnd");
  console.profileEnd();
  yield notified;
}
