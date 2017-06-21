/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that pretty printing is maintained across refreshes.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources;

function test() {
  // Wait for debugger panel to be fully set and break on debugger statement
  let options = {
    source: EXAMPLE_URL + "code_ugly.js",
    line: 2
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    testSourceIsUgly();
    const finished = waitForCaretUpdated(gPanel, 7);
    clickPrettyPrintButton();
    finished.then(testSourceIsPretty)
      .then(() => {
        const finished = waitForCaretUpdated(gPanel, 7);
        const reloaded = reloadActiveTab(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
        return Promise.all([finished, reloaded]);
      })
      .then(testSourceIsPretty)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .catch(aError => {
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

function testSourceIsPretty() {
  ok(gEditor.getText().includes("\n  "),
     "The source should be pretty printed.");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
});
