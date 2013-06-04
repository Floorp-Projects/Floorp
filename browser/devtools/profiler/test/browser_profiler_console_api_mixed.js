/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab, gPanel;

function test() {
  waitForExplicitFinish();

  setUp(URL, (tab, browser, panel) => {
    gTab = tab;
    gPanel = panel;

    openProfiler(tab, runTests);
  });
}

function runTests(toolbox) {
  let panel = toolbox.getPanel("jsprofiler");
  let getTitle = (uid) =>
    panel.document.querySelector("li#profile-" + uid + " > h1").textContent;

  panel.profiles.get(1).once("started", () => {
    is(getTitle(1), "Profile 1 *", "Profile 1 has a start next to it.");

    openConsole(gTab, (hud) => {
      panel.profiles.get(1).once("stopped", () => {
        is(getTitle(1), "Profile 1", "Profile 1 doesn't have a star next to it.");
        tearDown(gTab, () => gTab = gPanel = null);
      });

      hud.jsterm.execute("console.profileEnd()");
    });
  });

  sendFromProfile(1, "start");
}