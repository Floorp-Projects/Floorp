/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler displays and organizes the recording data in tabs.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, ProfileView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel, { waitForDisplay: true });

  is(ProfileView.tabCount, 1,
    "There should be one tab visible.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should be displayed in the profile view.");
  is($("#profile-content").selectedIndex, 0,
    "The first tab is selected.");

  ok($("#profile-content .tab-title-label").getAttribute("value")
    .match(/\d+ ms . \d+ ms/),
    "The recording's first tab title is correct.");

  yield teardown(panel);
  finish();
});
