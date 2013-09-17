/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that stackframes are scrollable.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gFrames, gFramesScrollingInterval;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 26).then(performTest);

    gDebuggee.gRecurseLimit = (gDebugger.gCallStackPageSize * 2) + 1;
    gDebuggee.recurse();
  });
}

function performTest() {
  is(gDebugger.gThreadClient.state, "paused",
    "Should only be getting stack frames while paused.");
  is(gFrames.itemCount, gDebugger.gCallStackPageSize,
    "Should have only the max limit of frames.");

  gDebugger.gThreadClient.addOneTimeListener("framesadded", () => {
    is(gFrames.itemCount, gDebugger.gCallStackPageSize * 2,
      "Should now have twice the max limit of frames.");

    gDebugger.gThreadClient.addOneTimeListener("framesadded", () => {
      is(gFrames.itemCount, gDebuggee.gRecurseLimit,
        "Should have reached the recurse limit.");

      gDebugger.gThreadClient.resume(() => {
        window.clearInterval(gFramesScrollingInterval);
        closeDebuggerAndFinish(gPanel);
      });
    });
  });

  gFramesScrollingInterval = window.setInterval(() => {
    gFrames.widget._list.scrollByIndex(-1);
  }, 100);
}

registerCleanupFunction(function() {
  window.clearInterval(gFramesScrollingInterval);
  gFramesScrollingInterval = null;

  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
});
