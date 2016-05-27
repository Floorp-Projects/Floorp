/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that stackframes are added when debugger is paused.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

var gTab, gPanel, gDebugger;
var gFrames, gClassicFrames;

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gClassicFrames = gDebugger.DebuggerView.StackFramesClassicList;

    waitForCaretAndScopes(gPanel, 14).then(performTest);
    callInTab(gTab, "simpleCall");
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

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
  gClassicFrames = null;
});
