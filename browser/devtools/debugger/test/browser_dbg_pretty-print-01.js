/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that clicking the pretty print button prettifies the source.
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
      .then(testSourceIsStillPretty)
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
  EventUtils.sendMouseEvent({ type: "click" },
                            gDebugger.document.getElementById("pretty-print"),
                            gDebugger);
}

function testSourceIsPretty() {
  ok(gEditor.getText().contains("\n    "),
     "The source should be pretty printed.")
}

function testSourceIsStillPretty() {
  const deferred = promise.defer();

  const { source } = gSources.selectedItem.attachment;
  gDebugger.DebuggerController.SourceScripts.getText(source).then(([, text]) => {
    ok(text.contains("\n    "),
       "Subsequent calls to getText return the pretty printed source.");
    deferred.resolve();
  });

  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
});
