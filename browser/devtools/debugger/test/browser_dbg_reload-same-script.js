/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the same source is shown after a page is reloaded.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";
const FIRST_URL = EXAMPLE_URL + "code_script-switching-01.js";
const SECOND_URL = EXAMPLE_URL + "code_script-switching-02.js";

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  let gTab, gDebuggee, gPanel, gDebugger;
  let gSources, gStep;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = aPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gStep = 0;

    waitForSourceShown(gPanel, FIRST_URL).then(performTest);
  });

  function performTest() {
    switch (gStep++) {
      case 0:
        testCurrentSource(FIRST_URL, "");
        reload().then(performTest);
        break;
      case 1:
        testCurrentSource(FIRST_URL);
        reload().then(performTest);
        break;
      case 2:
        testCurrentSource(FIRST_URL);
        switchAndReload(SECOND_URL).then(performTest);
        break;
      case 3:
        testCurrentSource(SECOND_URL);
        reload().then(performTest);
        break;
      case 4:
        testCurrentSource(SECOND_URL);
        reload().then(performTest);
        break;
      case 5:
        testCurrentSource(SECOND_URL);
        closeDebuggerAndFinish(gPanel);
        break;
    }
  }

  function reload() {
    return reloadActiveTab(gPanel, gDebugger.EVENTS.SOURCES_ADDED);
  }

  function switchAndReload(aUrl) {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(reload);
    gSources.selectedValue = aUrl;
    return finished;
  }

  function testCurrentSource(aUrl, aExpectedUrl = aUrl) {
    info("Currently preferred source: '" + gSources.preferredValue + "'.");
    info("Currently selected source: '" + gSources.selectedValue + "'.");

    is(gSources.preferredValue, aExpectedUrl,
      "The preferred source url wasn't set correctly (" + gStep + ").");
    is(gSources.selectedValue, aUrl,
      "The selected source isn't the correct one (" + gStep + ").");
  }
}
