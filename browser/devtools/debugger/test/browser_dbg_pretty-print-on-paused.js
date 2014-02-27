/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that pretty printing when the debugger is paused
 * does not switch away from the selected source.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print-on-paused.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gSources;

let gSecondSourceLabel = "code_ugly-2.js";

function test(){
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    gPanel.addBreakpoint({ url: gSources.values[0], line: 6 });

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 6)
      .then(testPaused)
      .then(() => {
        // Switch to the second source.
        let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
        gSources.selectedIndex = 1;
        return finished;
      })
      .then(testSecondSourceIsSelected)
      .then(() => {
        const finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
        clickPrettyPrintButton();
        testProgressBarShown();
        return finished;
      })
      .then(testSecondSourceIsStillSelected)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + DevToolsUtils.safeErrorString(aError));
      })

    gDebuggee.secondCall();
  });
}

function testPaused() {
  is(gDebugger.gThreadClient.paused, true,
    "The thread should be paused");
}

function testSecondSourceIsSelected() {
  ok(gSources.containsValue(EXAMPLE_URL + gSecondSourceLabel),
    "The second source should be selected.");
}

function clickPrettyPrintButton() {
  gDebugger.document.getElementById("pretty-print").click();
}

function testProgressBarShown() {
  const deck = gDebugger.document.getElementById("editor-deck");
  is(deck.selectedIndex, 2, "The progress bar should be shown");
}

function testSecondSourceIsStillSelected() {
  ok(gSources.containsValue(EXAMPLE_URL + gSecondSourceLabel),
    "The second source should still be selected.");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
});
