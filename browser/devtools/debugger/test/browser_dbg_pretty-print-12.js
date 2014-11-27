/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that we don't leave the pretty print button checked when we fail to
 * pretty print a source (because it isn't a JS file, for example).
 */

const TAB_URL = EXAMPLE_URL + "doc_blackboxing.html";

let gTab, gPanel, gDebugger;
let gEditor, gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceShown(gPanel, "")
      .then(() => {
        let shown = ensureSourceIs(gPanel, TAB_URL, true);
        gSources.selectedValue = getSourceActor(gSources, TAB_URL);
        return shown;
      })
      .then(clickPrettyPrintButton)
      .then(testButtonIsntChecked)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + DevToolsUtils.safeErrorString(aError));
      });
  });
}

function clickPrettyPrintButton() {
  gDebugger.document.getElementById("pretty-print").click();
}

function testButtonIsntChecked() {
  is(gDebugger.document.getElementById("pretty-print").checked, false,
     "The button shouldn't be checked after trying to pretty print a non-js file.");
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
});
