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

  panel.profiles.get(1).once("started", () => {
    is(getSidebarItem(1, panel).attachment.state, PROFILE_RUNNING);

    openConsole(gTab, (hud) => {
      panel.profiles.get(1).once("stopped", () => {
        is(getSidebarItem(1, panel).attachment.state, PROFILE_COMPLETED);
        tearDown(gTab, () => gTab = gPanel = null);
      });

      hud.jsterm.execute("console.profileEnd()");
    });
  });

  sendFromProfile(1, "start");
}