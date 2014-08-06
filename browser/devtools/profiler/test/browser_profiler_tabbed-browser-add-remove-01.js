/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if adding, naming and selecting tabs in the ProfileView works.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, $$, L10N, Prefs, ProfileView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel, { waitForDisplay: true });

  let tabIndex = ProfileView.addTab();
  is(tabIndex, 1,
    "A new tab was successfully added to the tabbed browser.");

  is($("#profile-content tabs").childNodes.length, 2,
    "There should be two tabs in the tabbed browser.");
  is($("#profile-content tabpanels").childNodes.length, 2,
    "There should be two tabpanels in the tabbed browser.");

  is($("#profile-content").selectedIndex, 0,
    "The first tab should still be selected in the tabbed browser (1).");
  is($("#profile-content").selectedTab, $$("#profile-content tab")[0],
    "The first tab should still be selected in the tabbed browser (2).");

  ProfileView.nameTab(tabIndex, 12.34, 56.78);

  ok($$("#profile-content .tab-title-label")[1].getAttribute("value"),
    L10N.getFormatStr("profile.tab", "12.34", "56.78"),
    "The newly added tab's title was successfully changed.");

  ProfileView.selectTab(tabIndex);

  is($("#profile-content").selectedIndex, 1,
    "The second tab is now selected in the tabbed browser (1).");
  is($("#profile-content").selectedTab, $$("#profile-content tab")[1],
    "The second tab is now selected in the tabbed browser (2).");

  yield teardown(panel);
  finish();
});
