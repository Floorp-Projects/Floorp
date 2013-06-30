/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab, gPanel;

function test() {
  waitForExplicitFinish();

  setUp(URL, (tab, browser, panel) => {
    gTab = tab;
    gPanel = panel;

    openConsole(tab, testConsoleProfile);
  });
}

function testConsoleProfile(hud) {
  hud.jsterm.clearOutput(true);

  let profilesStarted = 0;

  gPanel.once("parsed", () => {
    let profile = gPanel.activeProfile;

    is(profile.name, "Profile 1", "Profile name is OK");
    is(gPanel.sidebar.selectedItem, gPanel.sidebar.getItemByProfile(profile), "Sidebar is OK");
    is(gPanel.sidebar.selectedItem.attachment.state, PROFILE_COMPLETED);
    tearDown(gTab, () => gTab = gPanel = null);
  });

  hud.jsterm.execute("console.profile()");
  hud.jsterm.execute("console.profileEnd()");
}