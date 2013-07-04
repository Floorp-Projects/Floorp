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

  let profilesStarted = 0;

  function endProfile() {
    if (++profilesStarted < 2)
      return;

    gPanel.controller.off("profileStart", endProfile);
    gPanel.controller.once("profileEnd", () => openProfiler(gTab, checkProfiles));
    hud.jsterm.execute("console.profileEnd('Second')");
  }

  gPanel.controller.on("profileStart", endProfile);
  hud.jsterm.execute("console.profile('Second')");
  hud.jsterm.execute("console.profile('Third')");
}

function checkProfiles(toolbox) {
  let panel = toolbox.getPanel("jsprofiler");

  is(getSidebarItem(1, panel).attachment.name, "Second", "Name in sidebar is OK");
  is(getSidebarItem(1, panel).attachment.state, PROFILE_COMPLETED, "State in sidebar is OK");

  // Make sure we can still stop profiles via the queue pop.

  gPanel.controller.once("profileEnd", () => {
    openProfiler(gTab, () => {
      is(getSidebarItem(2, panel).attachment.state, PROFILE_COMPLETED, "State in sidebar is OK");
      tearDown(gTab, () => gTab = gPanel = null);
    });
  });

  openConsole(gTab, (hud) => hud.jsterm.execute("console.profileEnd()"));
}