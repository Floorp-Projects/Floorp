/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests related to source panel keyboard shortcut bindings
 */

const TAB_URL = EXAMPLE_URL + "doc_function-search.html";
const SCRIPT_URI = EXAMPLE_URL + "code_function-search-01.js";

function test() {
  let gTab, gPanel, gDebugger, gSources;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceShown(gPanel, "-01.js")
      .then(testCopyURLShortcut)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function testCopyURLShortcut() {
    return waitForClipboardPromise(sendCopyShortcut, SCRIPT_URI);
  }

  function sendCopyShortcut() {
    EventUtils.synthesizeKey("C", { accelKey: true }, gDebugger);
  }
}
