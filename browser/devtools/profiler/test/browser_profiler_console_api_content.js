/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";
const BASE = "http://example.com/browser/browser/devtools/profiler/test/";
const PAGE = BASE + "mock_console_api.html";

let gTab, gPanel, gToolbox;

function test() {
  waitForExplicitFinish();

  setUp(URL, (tab, browser, panel) => {
    gTab = tab;
    gPanel = panel;

    openProfiler(tab, (toolbox) => {
      gToolbox = toolbox;
      loadUrl(PAGE, tab, () => {
        gPanel.sidebar.on("stateChanged", (_, item) => {
          if (item.attachment.state !== PROFILE_COMPLETED)
            return;

          runTests();
        });
      });
    });
 });
}

function runTests() {
  is(getSidebarItem(1).attachment.state, PROFILE_COMPLETED);

  gPanel.once("parsed", () => {
    function assertSampleAndFinish() {
      let [win,doc] = getProfileInternals();
      let sample = doc.getElementsByClassName("samplePercentage");

      if (sample.length <= 0)
        return void setTimeout(assertSampleAndFinish, 100);

      ok(sample.length > 0, "We have Cleopatra UI displayed");
      tearDown(gTab, () => {
        gTab = null;
        gPanel = null;
        gToolbox = null;
      });
    }

    assertSampleAndFinish();
  });

  let profile = gPanel.profiles.get(2);
  gPanel.sidebar.selectedItem = gPanel.sidebar.getItemByProfile(profile);
}