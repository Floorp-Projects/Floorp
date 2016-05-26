/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that closing a tab with the debugger in a paused state exits cleanly.
 */

var gTab, gPanel, gDebugger;

const TAB_URL = EXAMPLE_URL + "doc_inline-debugger-statement.html";

function test() {
  const options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    testCleanExit();
  });
}

function testCleanExit() {
  promise.all([
    waitForSourceAndCaretAndScopes(gPanel, ".html", 16),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_REFILLED)
  ]).then(() => {
    is(gDebugger.gThreadClient.paused, true,
      "Should be paused after the debugger statement.");
  }).then(() => closeDebuggerAndFinish(gPanel, { whilePaused: true }));

  callInTab(gTab, "runDebuggerStatement");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
});
