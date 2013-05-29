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

  function profileEnd(_, uid) {
    let profile = gPanel.profiles.get(uid);

    profile.once("started", () => {
      if (++profilesStarted < 2)
        return;

      gPanel.off("profileCreated", profileEnd);
      gPanel.profiles.get(2).once("stopped", () => {
        openProfiler(gTab, checkProfiles);
      });

      hud.jsterm.execute("console.profileEnd('Second')");
    });
  }

  gPanel.on("profileCreated", profileEnd);
  hud.jsterm.execute("console.profile('Second')");
  hud.jsterm.execute("console.profile('Third')");
}

function checkProfiles(toolbox) {
  let panel = toolbox.getPanel("jsprofiler");
  let getTitle = (uid) =>
    panel.document.querySelector("li#profile-" + uid + " > h1").textContent;

  is(getTitle(1), "Profile 1", "Profile 1 doesn't have a star next to it.");
  is(getTitle(2), "Second", "Second doesn't have a star next to it.");
  is(getTitle(3), "Third *", "Third does have a star next to it.");

  // Make sure we can still stop profiles via the queue pop.

  gPanel.profiles.get(3).once("stopped", () => {
    openProfiler(gTab, () => {
      is(getTitle(3), "Third", "Third doesn't have a star next to it.");
      tearDown(gTab, () => gTab = gPanel = null);
    });
  });

  openConsole(gTab, (hud) => hud.jsterm.execute("console.profileEnd()"));
}