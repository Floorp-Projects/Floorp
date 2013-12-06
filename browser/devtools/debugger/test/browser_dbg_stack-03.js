/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that stackframes are scrollable.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gFrames, gClassicFrames, gFramesScrollingInterval;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gClassicFrames = gDebugger.DebuggerView.StackFramesClassicList;

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
  is(gClassicFrames.itemCount, gDebugger.gCallStackPageSize,
    "Should have only the max limit of frames in the mirrored view as well.")

  gDebugger.gThreadClient.addOneTimeListener("framesadded", () => {
    is(gFrames.itemCount, gDebugger.gCallStackPageSize * 2,
      "Should now have twice the max limit of frames.");
    is(gClassicFrames.itemCount, gDebugger.gCallStackPageSize * 2,
      "Should now have twice the max limit of frames in the mirrored view as well.");

    gDebugger.gThreadClient.addOneTimeListener("framesadded", () => {
      is(gFrames.itemCount, gDebuggee.gRecurseLimit,
        "Should have reached the recurse limit.");
      is(gClassicFrames.itemCount, gDebuggee.gRecurseLimit,
        "Should have reached the recurse limit in the mirrored view as well.");

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
  gClassicFrames = null;
});
