/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler correctly handles multiple recordings and can
 * successfully switch between them.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, EVENTS, RecordingsListView, ProfileView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel, { waitForDisplay: true });

  yield startRecording(panel);
  yield stopRecording(panel, { waitForDisplay: true });

  is(RecordingsListView.itemCount, 2,
    "There should be two recordings visible.");
  is(RecordingsListView.selectedIndex, 1,
    "The second recording item should be selected.");

  is(ProfileView.tabCount, 1,
    "There should be one tab visible.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should be displayed in the profile view.");

  let recordingDisplayed = panel.panelWin.once(EVENTS.RECORDING_DISPLAYED);
  RecordingsListView.selectedIndex = 0;
  yield recordingDisplayed;

  is(RecordingsListView.itemCount, 2,
    "There should still be two recordings visible.");
  is(RecordingsListView.selectedIndex, 0,
    "The first recording item should be selected.");

  is(ProfileView.tabCount, 1,
    "There should still be one tab visible.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should still be displayed in the profile view.");

  let emptyNoticeDisplayed = panel.panelWin.once(EVENTS.EMPTY_NOTICE_SHOWN);
  RecordingsListView.selectedIndex = -1;
  yield emptyNoticeDisplayed;

  is(RecordingsListView.itemCount, 2,
    "There should still be two recordings visible.");
  is(RecordingsListView.selectedItem, null,
    "No recording item should be selected.");

  is($("#profile-pane").selectedPanel, $("#empty-notice"),
    "The empty notice should still be displayed in the profile view.");

  yield teardown(panel);
  finish();
});
