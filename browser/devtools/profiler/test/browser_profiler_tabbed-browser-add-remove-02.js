/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if removing tabs in the ProfileView works.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, $$, L10N, Prefs, ProfileView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel, { waitForDisplay: true });

  let newTab1 = ProfileView.addTab();
  let newTab2 = ProfileView.addTab();
  let newTab3 = ProfileView.addTab();

  is($("#profile-content tabs").childNodes.length, 4,
    "There should be four tabs in the tabbed browser.");
  is($("#profile-content tabpanels").childNodes.length, 4,
    "There should be four tabpanels in the tabbed browser.");

  is($("#profile-content").selectedIndex, 0,
    "The first tab should still be selected in the tabbed browser (1).");
  is($("#profile-content").selectedTab, $$("#profile-content tab")[0],
    "The first tab should still be selected in the tabbed browser (2).");

  ProfileView.removeTabsAfter(newTab1);

  is($("#profile-content tabs").childNodes.length, 2,
    "There should be two tabs in the tabbed browser now.");
  is($("#profile-content tabpanels").childNodes.length, 2,
    "There should be two tabpanels in the tabbed browser now.");

  is($("#profile-content").selectedIndex, 0,
    "The first tab should still be selected in the tabbed browser (3).");
  is($("#profile-content").selectedTab, $$("#profile-content tab")[0],
    "The first tab should still be selected in the tabbed browser (4).");

  ProfileView.removeAllTabs();

  is($("#profile-content tabs").childNodes.length, 0,
    "There should be no tabs in the tabbed browser now.");
  is($("#profile-content tabpanels").childNodes.length, 0,
    "There should be no tabpanels in the tabbed browser now.");

  is($("#profile-content").selectedIndex, -1,
    "No tab should be selected in the tabbed browser (3).");
  is($("#profile-content").selectedTab, null,
    "No tab should be selected in the tabbed browser (4).");

  yield teardown(panel);
  finish();
});
