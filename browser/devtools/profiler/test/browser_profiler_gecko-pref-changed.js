/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profile view is rebuilt every time the
 * devtools.profiler.ui.show-platform-data pref is changed.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, EVENTS, Prefs, RecordingsListView, ProfileView } = panel.panelWin;

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

  let emptyNoticeDisplayed = panel.panelWin.once(EVENTS.EMPTY_NOTICE_SHOWN);
  let tabbedBrowserRedisplayed = panel.panelWin.once(EVENTS.TABBED_BROWSER_SHOWN);
  let recordingRedisplayed = panel.panelWin.once(EVENTS.RECORDING_DISPLAYED);

  let prevPrefValue = Prefs.showPlatformData;
  Prefs.showPlatformData ^= 1;

  yield emptyNoticeDisplayed;
  yield tabbedBrowserRedisplayed;
  yield recordingRedisplayed;
  ok(true, "The profile view was rebuilt.");

  is(RecordingsListView.itemCount, 1,
    "There should still be one recording visible.");
  ok(!$(".side-menu-widget-empty-text"),
    "There still shouldn't be any empty text displayed in the recordings list.");

  is(ProfileView.tabCount, 1,
    "There should still be one tab visible.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should still be displayed in the profile view.");

  yield teardown(panel);
  finish();
});
