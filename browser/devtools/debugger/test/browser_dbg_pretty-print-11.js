/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that pretty printing is maintained across refreshes.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceShown(gPanel, "code_ugly.js")
      .then(testSourceIsUgly)
      .then(() => {
        const finished = waitForSourceShown(gPanel, "code_ugly.js");
        clickPrettyPrintButton();
        return finished;
      })
      .then(testSourceIsPretty)
      .then(reloadActiveTab.bind(null, gPanel, gDebugger.EVENTS.SOURCE_SHOWN))
      .then(testSourceIsPretty)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + DevToolsUtils.safeErrorString(aError));
      });
  });
}

function testSourceIsUgly() {
  ok(!gEditor.getText().contains("\n    "),
     "The source shouldn't be pretty printed yet.");
}

function clickPrettyPrintButton() {
  gDebugger.document.getElementById("pretty-print").click();
}

function testSourceIsPretty() {
  ok(gEditor.getText().contains("\n    "),
     "The source should be pretty printed.")
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
});
