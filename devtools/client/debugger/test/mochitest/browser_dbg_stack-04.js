/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that stackframes are cleared after resume.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

var gTab, gPanel, gDebugger;
var gFrames, gClassicFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gClassicFrames = gDebugger.DebuggerView.StackFramesClassicList;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 1).then(performTest);
    callInTab(gTab, "evalCall");
  });
}

function performTest() {
  is(gDebugger.gThreadClient.state, "paused",
    "Should only be getting stack frames while paused.");
  is(gFrames.itemCount, 2,
    "Should have two frames.");
  is(gClassicFrames.itemCount, 2,
    "Should also have two frames in the mirrored view.");

  gDebugger.once(gDebugger.EVENTS.AFTER_FRAMES_CLEARED, () => {
    is(gFrames.itemCount, 0,
      "Should have no frames after resume.");
    is(gClassicFrames.itemCount, 0,
      "Should also have no frames in the mirrored view after resume.");

    closeDebuggerAndFinish(gPanel);
  }, true);

  gDebugger.gThreadClient.resume();
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
  gClassicFrames = null;
});
