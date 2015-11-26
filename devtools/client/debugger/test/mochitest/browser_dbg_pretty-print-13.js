/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that clicking the pretty print button prettifies the source, even
 * when the source URL does not end in ".js", but the content type is
 * JavaScript.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print-3.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    promise.all([waitForSourceShown(gPanel, "code_ugly-8"),
                 waitForEditorLocationSet(gPanel)])
      .then(testSourceIsUgly)
      .then(() => {
        const finished = waitForSourceShown(gPanel, "code_ugly-8");
        clickPrettyPrintButton();
        testProgressBarShown();
        return finished;
      })
      .then(testSourceIsPretty)
      .then(testEditorShown)
      .then(testSourceIsStillPretty)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + DevToolsUtils.safeErrorString(aError));
      });
  });
}

function testSourceIsUgly() {
  ok(!gEditor.getText().includes("\n  "),
     "The source shouldn't be pretty printed yet.");
}

function clickPrettyPrintButton() {
  gDebugger.document.getElementById("pretty-print").click();
}

function testProgressBarShown() {
  const deck = gDebugger.document.getElementById("editor-deck");
  is(deck.selectedIndex, 2, "The progress bar should be shown");
}

function testSourceIsPretty() {
  ok(gEditor.getText().includes("\n  "),
     "The source should be pretty printed.")
}

function testEditorShown() {
  const deck = gDebugger.document.getElementById("editor-deck");
  is(deck.selectedIndex, 0, "The editor should be shown");
}

function testSourceIsStillPretty() {
  const deferred = promise.defer();

  const { source } = gSources.selectedItem.attachment;
  gDebugger.DebuggerController.SourceScripts.getText(source).then(([, text]) => {
    ok(text.includes("\n  "),
       "Subsequent calls to getText return the pretty printed source.");
    deferred.resolve();
  });

  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
});
