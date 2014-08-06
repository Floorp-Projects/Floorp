/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is able to end a simple recording.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, L10N, RecordingsListView, ProfileView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel, { waitForDisplay: true });

  is(RecordingsListView.itemCount, 1,
    "There should be one recording visible.");
  ok(!$(".side-menu-widget-empty-text"),
    "There shouldn't be any empty text displayed in the recordings list.");

  is(ProfileView.tabCount, 1,
    "There should be one tab visible.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should be displayed in the profile view.");

  let recordingItem = RecordingsListView.selectedItem;
  is(recordingItem, RecordingsListView.items[0],
    "The first and only recording item should be automatically selected.");

  ok(recordingItem.attachment.recordingDuration > 0,
    "The recording duration appears to be correct.");
  is(recordingItem.attachment.profileLabel, undefined,
    "The profile label should be undefined for non-console recordings.");
  ok(recordingItem.attachment.profilerData,
    "The profiler data appears to be correct.");
  ok(recordingItem.attachment.ticksData,
    "The ticks data appears to be correct.");

  is($(".recording-item-title", recordingItem.target).getAttribute("value"),
    L10N.getFormatStr("recordingsList.itemLabel", 1),
    "The recording item's title is correct.");

  ok($(".recording-item-duration", recordingItem.target).getAttribute("value")
    .match(/\d+ ms/),
    "The recording item's duration is correct.");

  is($(".recording-item-save", recordingItem.target).getAttribute("value"),
    L10N.getStr("recordingsList.saveLabel"),
    "The recording item's save link is correct.");

  yield teardown(panel);
  finish();
});
