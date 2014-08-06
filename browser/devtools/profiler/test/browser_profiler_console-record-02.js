/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is populated by in-progress console recordings
 * when it is opened.
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

  yield toolbox.selectTool("jsprofiler");
  let profilerPanel = toolbox.getCurrentPanel();
  let { $, L10N, RecordingsListView, ProfileView } = profilerPanel.panelWin;

  is(RecordingsListView.itemCount, 2,
    "There should be two recordings visible.");
  ok(!$(".side-menu-widget-empty-text"),
    "There shouldn't be any empty text displayed in the recordings list.");

  is(ProfileView.tabCount, 0,
    "There shouldn't be any tabs visible yet.");
  is($("#profile-pane").selectedPanel, $("#recording-notice"),
    "There should be a recording notice displayed in the profile view.");

  is(RecordingsListView.items[0], RecordingsListView.selectedItem,
    "The first recording item should be automatically selected.");

  is(RecordingsListView.items[0].attachment.profilerData.profileLabel, "1",
    "The profile label for the first recording is correct.");
  ok(RecordingsListView.items[0].isRecording,
    "The 'isRecording' flag for the first recording is correct.");

  is(RecordingsListView.items[1].attachment.profilerData.profileLabel, "2",
    "The profile label for the second recording is correct.");
  ok(RecordingsListView.items[1].isRecording,
    "The 'isRecording' flag for the second recording is correct.");

  let firstTarget = RecordingsListView.items[0].target;
  let secondTarget = RecordingsListView.items[1].target;

  is($(".recording-item-title", firstTarget).getAttribute("value"), "1",
    "The first recording item's title is correct.");
  is($(".recording-item-title", secondTarget).getAttribute("value"), "2",
    "The second recording item's title is correct.");

  is($(".recording-item-duration", firstTarget).getAttribute("value"),
    L10N.getStr("recordingsList.recordingLabel"),
    "The first recording item's duration is correct.");
 is($(".recording-item-duration", secondTarget).getAttribute("value"),
    L10N.getStr("recordingsList.recordingLabel"),
    "The second recording item's duration is correct.");

  is($(".recording-item-save", firstTarget).getAttribute("value"), "",
    "The first recording item's save link should be invisible.");
  is($(".recording-item-save", secondTarget).getAttribute("value"), "",
    "The second recording item's save link should be invisible.");

  yield teardown(profilerPanel);
  finish();
});

function* consoleProfile(connection, label) {
  let notified = connection.once("profile");
  console.profile(label);
  yield notified;
}
