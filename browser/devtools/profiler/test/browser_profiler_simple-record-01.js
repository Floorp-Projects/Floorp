/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is able to start a simple recording.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, L10N, RecordingsListView, ProfileView } = panel.panelWin;

  is(RecordingsListView.itemCount, 0,
    "There should be no recordings visible yet.");
  ok($(".side-menu-widget-empty-text"),
    "There should be some empty text displayed in the recordings list.");

  is(ProfileView.tabCount, 0,
    "There shouldn't be any tabs visible yet.");
  is($("#profile-pane").selectedPanel, $("#empty-notice"),
    "There should be an empty notice displayed in the profile view.");

  yield startRecording(panel);

  is(RecordingsListView.itemCount, 1,
    "There should be one recording visible now.");
  ok(!$(".side-menu-widget-empty-text"),
    "There shouldn't be any empty text displayed in the recordings list.");

  is(ProfileView.tabCount, 0,
    "There still shouldn't be any tabs visible yet.");
  is($("#profile-pane").selectedPanel, $("#recording-notice"),
    "There should be a recording notice displayed in the profile view.");

  let recordingItem = RecordingsListView.selectedItem;
  is(recordingItem, RecordingsListView.items[0],
    "The first and only recording item should be automatically selected.");

  is($(".recording-item-title", recordingItem.target).getAttribute("value"),
    L10N.getFormatStr("recordingsList.itemLabel", 1),
    "The recording item's title is correct.");

  is($(".recording-item-duration", recordingItem.target).getAttribute("value"),
    L10N.getStr("recordingsList.recordingLabel"),
    "The recording item's duration is correct.");

  is($(".recording-item-save", recordingItem.target).getAttribute("value"), "",
    "The recording item's save link should be invisible.");

  yield teardown(panel);
  finish();
});
