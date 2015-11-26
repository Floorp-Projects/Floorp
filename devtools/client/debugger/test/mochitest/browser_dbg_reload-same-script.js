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

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = aPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require('./content/queries');
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;
    let gStep = 0;

    function reloadPage() {
      const loaded = waitForDispatch(gPanel, gDebugger.constants.LOAD_SOURCES);
      reload(gPanel);
      return loaded;
    }

    function switchAndReload(aUrl) {
      actions.selectSource(getSourceForm(gSources, aUrl));
      return reloadPage();
    }

    function testCurrentSource(aUrl, aExpectedUrl = aUrl) {
      const prefSource = getSourceURL(gSources, gSources.preferredValue);
      const selSource = getSourceURL(gSources, gSources.selectedValue);

      info("Currently preferred source: '" + prefSource + "'.");
      info("Currently selected source: '" + selSource + "'.");

      is(prefSource, aExpectedUrl,
         "The preferred source url wasn't set correctly (" + gStep + ").");
      is(selSource, aUrl,
         "The selected source isn't the correct one (" + gStep + ").");
    }

    function performTest() {
      switch (gStep++) {
      case 0:
        testCurrentSource(FIRST_URL, null);
        reloadPage().then(performTest);
        break;
      case 1:
        testCurrentSource(FIRST_URL);
        reloadPage().then(performTest);
        break;
      case 2:
        testCurrentSource(FIRST_URL);
        switchAndReload(SECOND_URL).then(performTest);
        break;
      case 3:
        testCurrentSource(SECOND_URL);
        reloadPage().then(performTest);
        break;
      case 4:
        testCurrentSource(SECOND_URL);
        reloadPage().then(performTest);
        break;
      case 5:
        testCurrentSource(SECOND_URL);
        closeDebuggerAndFinish(gPanel);
        break;
      }
    }

    waitForSourceShown(gPanel, FIRST_URL).then(performTest);
  });
}
