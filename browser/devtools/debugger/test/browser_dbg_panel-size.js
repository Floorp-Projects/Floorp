/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the sources and instruments panels widths are properly
 * remembered when the debugger closes.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  let gTab, gDebuggee, gPanel, gDebugger;
  let gPrefs, gSources, gInstruments;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gPrefs = gDebugger.Prefs;
    gSources = gDebugger.document.getElementById("sources-pane");
    gInstruments = gDebugger.document.getElementById("instruments-pane");

    waitForSourceShown(gPanel, ".html").then(performTest);
  });

  function performTest() {
    let preferredSw = Services.prefs.getIntPref("devtools.debugger.ui.panes-sources-width");
    let preferredIw = Services.prefs.getIntPref("devtools.debugger.ui.panes-instruments-width");
    let someWidth1, someWidth2;

    do {
      someWidth1 = parseInt(Math.random() * 200) + 100;
      someWidth2 = parseInt(Math.random() * 300) + 100;
    } while ((someWidth1 == preferredSw) || (someWidth2 == preferredIw));

    info("Preferred sources width: " + preferredSw);
    info("Preferred instruments width: " + preferredIw);
    info("Generated sources width: " + someWidth1);
    info("Generated instruments width: " + someWidth2);

    ok(gPrefs.sourcesWidth,
      "The debugger preferences should have a saved sourcesWidth value.");
    ok(gPrefs.instrumentsWidth,
      "The debugger preferences should have a saved instrumentsWidth value.");

    is(gPrefs.sourcesWidth, preferredSw,
      "The debugger preferences should have a correct sourcesWidth value.");
    is(gPrefs.instrumentsWidth, preferredIw,
      "The debugger preferences should have a correct instrumentsWidth value.");

    is(gSources.getAttribute("width"), gPrefs.sourcesWidth,
      "The sources pane width should be the same as the preferred value.");
    is(gInstruments.getAttribute("width"), gPrefs.instrumentsWidth,
      "The instruments pane width should be the same as the preferred value.");

    gSources.setAttribute("width", someWidth1);
    gInstruments.setAttribute("width", someWidth2);

    is(gPrefs.sourcesWidth, preferredSw,
      "The sources pane width pref should still be the same as the preferred value.");
    is(gPrefs.instrumentsWidth, preferredIw,
      "The instruments pane width pref should still be the same as the preferred value.");

    isnot(gSources.getAttribute("width"), gPrefs.sourcesWidth,
      "The sources pane width should not be the preferred value anymore.");
    isnot(gInstruments.getAttribute("width"), gPrefs.instrumentsWidth,
      "The instruments pane width should not be the preferred value anymore.");

    teardown(gPanel).then(() => {
      is(gPrefs.sourcesWidth, someWidth1,
        "The sources pane width should have been saved by now.");
      is(gPrefs.instrumentsWidth, someWidth2,
        "The instruments pane width should have been saved by now.");

      // Cleanup after ourselves!
      Services.prefs.setIntPref("devtools.debugger.ui.panes-sources-width", preferredSw);
      Services.prefs.setIntPref("devtools.debugger.ui.panes-instruments-width", preferredIw);

      finish();
    });
  }
}
