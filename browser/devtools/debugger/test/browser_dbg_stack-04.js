/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that stackframes are cleared after resume.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 18).then(performTest);
    gDebuggee.evalCall();
  });
}

function performTest() {
  is(gDebugger.gThreadClient.state, "paused",
    "Should only be getting stack frames while paused.");
  is(gFrames.itemCount, 2,
    "Should have two frames.");

  gDebugger.once(gDebugger.EVENTS.AFTER_FRAMES_CLEARED, () => {
    is(gFrames.itemCount, 0,
      "Should have no frames after resume.");

    closeDebuggerAndFinish(gPanel);
  }, true);

  gDebugger.gThreadClient.resume();
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
});
