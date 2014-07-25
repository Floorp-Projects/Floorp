/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that we disable the pretty print button for black boxed sources,
 * and that clicking it doesn't do anything.
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
      .then(toggleBlackBoxing.bind(null, gPanel))
      .then(clickPrettyPrintButton)
      .then(testSourceIsStillUgly)
      .then(() => closeDebuggerAndFinish(gPanel))
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
  // Wait a tick before clicking to make sure the frontend's blackboxchange
  // handlers have finished.
  return new Promise(resolve => {
    gDebugger.document.getElementById("pretty-print").click();
    resolve();
  });
}

function testSourceIsStillUgly() {
  const { source } = gSources.selectedItem.attachment;
  return gDebugger.DebuggerController.SourceScripts.getText(source).then(([, text]) => {
    ok(!text.contains("\n    "));
  });
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
});
