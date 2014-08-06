/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profile view correctly displays its panels and emits
 * the appropriate events.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, EVENTS, ProfileView } = panel.panelWin;

  is($("#profile-pane").selectedPanel, $("#empty-notice"),
    "The empty notice should initially be displayed in the profile view.");

  let recordingNoticeDisplayed = panel.panelWin.once(EVENTS.RECORDING_NOTICE_SHOWN);
  ProfileView.showRecordingNotice();
  yield recordingNoticeDisplayed;

  is($("#profile-pane").selectedPanel, $("#recording-notice"),
    "The recording notice should now be displayed in the profile view.");

  let loadingNoticeDisplayed = panel.panelWin.once(EVENTS.LOADING_NOTICE_SHOWN);
  ProfileView.showLoadingNotice();
  yield loadingNoticeDisplayed;

  is($("#profile-pane").selectedPanel, $("#loading-notice"),
    "The loading notice should now be displayed in the profile view.");

  let tabbedBrowserDisplayed = panel.panelWin.once(EVENTS.TABBED_BROWSER_SHOWN);
  ProfileView.showTabbedBrowser();
  yield tabbedBrowserDisplayed;

  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should now be displayed in the profile view.");

  let emptyNoticeDisplayed = panel.panelWin.once(EVENTS.EMPTY_NOTICE_SHOWN);
  ProfileView.showEmptyNotice();
  yield emptyNoticeDisplayed;

  is($("#profile-pane").selectedPanel, $("#empty-notice"),
    "The empty notice should now be redisplayed in the profile view.");

  yield teardown(panel);
  finish();
});
