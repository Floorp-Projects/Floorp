/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the sources and instruments panels widths are properly
 * remembered when the debugger closes.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gPrefs, gSources, gInstruments;

  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gPrefs = gDebugger.Prefs;
    gSources = gDebugger.document.getElementById("workers-and-sources-pane");
    gInstruments = gDebugger.document.getElementById("instruments-pane");

    performTest();
  });

  function performTest() {
    let preferredWsw = Services.prefs.getIntPref("devtools.debugger.ui.panes-workers-and-sources-width");
    let preferredIw = Services.prefs.getIntPref("devtools.debugger.ui.panes-instruments-width");
    let someWidth1, someWidth2;

    do {
      someWidth1 = parseInt(Math.random() * 200) + 100;
      someWidth2 = parseInt(Math.random() * 300) + 100;
    } while ((someWidth1 == preferredWsw) || (someWidth2 == preferredIw));

    info("Preferred sources width: " + preferredWsw);
    info("Preferred instruments width: " + preferredIw);
    info("Generated sources width: " + someWidth1);
    info("Generated instruments width: " + someWidth2);

    ok(gPrefs.workersAndSourcesWidth,
      "The debugger preferences should have a saved workersAndSourcesWidth value.");
    ok(gPrefs.instrumentsWidth,
      "The debugger preferences should have a saved instrumentsWidth value.");

    is(gPrefs.workersAndSourcesWidth, preferredWsw,
      "The debugger preferences should have a correct workersAndSourcesWidth value.");
    is(gPrefs.instrumentsWidth, preferredIw,
      "The debugger preferences should have a correct instrumentsWidth value.");

    is(gSources.getAttribute("width"), gPrefs.workersAndSourcesWidth,
      "The workers and sources pane width should be the same as the preferred value.");
    is(gInstruments.getAttribute("width"), gPrefs.instrumentsWidth,
      "The instruments pane width should be the same as the preferred value.");

    gSources.setAttribute("width", someWidth1);
    gInstruments.setAttribute("width", someWidth2);

    is(gPrefs.workersAndSourcesWidth, preferredWsw,
      "The workers and sources pane width pref should still be the same as the preferred value.");
    is(gPrefs.instrumentsWidth, preferredIw,
      "The instruments pane width pref should still be the same as the preferred value.");

    isnot(gSources.getAttribute("width"), gPrefs.workersAndSourcesWidth,
      "The workers and sources pane width should not be the preferred value anymore.");
    isnot(gInstruments.getAttribute("width"), gPrefs.instrumentsWidth,
      "The instruments pane width should not be the preferred value anymore.");

    teardown(gPanel).then(() => {
      is(gPrefs.workersAndSourcesWidth, someWidth1,
        "The workers and sources pane width should have been saved by now.");
      is(gPrefs.instrumentsWidth, someWidth2,
        "The instruments pane width should have been saved by now.");

      // Cleanup after ourselves!
      Services.prefs.setIntPref("devtools.debugger.ui.panes-workers-and-sources-width", preferredWsw);
      Services.prefs.setIntPref("devtools.debugger.ui.panes-instruments-width", preferredIw);

      finish();
    });
  }
}
