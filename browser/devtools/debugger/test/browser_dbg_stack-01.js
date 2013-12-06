/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that stackframes are added when debugger is paused.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gFrames, gClassicFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gClassicFrames = gDebugger.DebuggerView.StackFramesClassicList;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 14).then(performTest);
    gDebuggee.simpleCall();
  });
}

function performTest() {
  is(gDebugger.gThreadClient.state, "paused",
    "Should only be getting stack frames while paused.");
  is(gFrames.itemCount, 1,
    "Should have only one frame.");
  is(gClassicFrames.itemCount, 1,
    "Should also have only one frame in the mirrored view.");

  resumeDebuggerThenCloseAndFinish(gPanel);
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
  gClassicFrames = null;
});
