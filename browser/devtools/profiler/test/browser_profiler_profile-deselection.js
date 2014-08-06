/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler shows the appropriate notice when a selection
 * from the recordings list is lost.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, EVENTS, RecordingsListView } = panel.panelWin;

  is(RecordingsListView.itemCount, 0,
    "There should be no recordings visible yet.");
  is($("#profile-pane").selectedPanel, $("#empty-notice"),
    "There should be an empty notice displayed in the profile view.");

  yield startRecording(panel);
  yield stopRecording(panel, { waitForDisplay: true });

  is(RecordingsListView.itemCount, 1,
    "There should be one recording visible now.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should be displayed in the profile view.");

  RecordingsListView.selectedItem = null;

  is(RecordingsListView.itemCount, 1,
    "There should still be one recording visible now.");
  is($("#profile-pane").selectedPanel, $("#empty-notice"),
    "There should be an empty notice displayed in the profile view again.");

  yield teardown(panel);
  finish();
});
