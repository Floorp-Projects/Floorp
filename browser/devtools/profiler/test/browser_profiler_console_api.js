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

  // Here we start two named profiles and then end one of them.
  // profileEnd, when name is not provided, simply pops the latest
  // profile.

  let profilesStarted = 0;

  function profileEnd(_, uid) {
    let profile = gPanel.profiles.get(uid);

    profile.once("started", () => {
      if (++profilesStarted < 2)
        return;

      gPanel.off("profileCreated", profileEnd);
      gPanel.profiles.get(3).once("stopped", () => {
        openProfiler(gTab, checkProfiles);
      });

      hud.jsterm.execute("console.profileEnd()");
    });
  }

  gPanel.on("profileCreated", profileEnd);
  hud.jsterm.execute("console.profile()");
  hud.jsterm.execute("console.profile()");
}

function checkProfiles(toolbox) {
  let panel = toolbox.getPanel("jsprofiler");

  is(getSidebarItem(1, panel).attachment.state, PROFILE_IDLE);
  is(getSidebarItem(2, panel).attachment.state, PROFILE_RUNNING);
  is(getSidebarItem(3, panel).attachment.state, PROFILE_COMPLETED);

  // Make sure we can still stop profiles via the UI.

  gPanel.profiles.get(2).once("stopped", () => {
    is(getSidebarItem(2, panel).attachment.state, PROFILE_COMPLETED);
    tearDown(gTab, () => gTab = gPanel = null);
  });

  sendFromProfile(2, "stop");
}