/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that anonymous functions appear in the stack frame list with either
 * their displayName property or a SpiderMonkey-inferred name.
 */

const TAB_URL = EXAMPLE_URL + "doc_function-display-name.html";

var gTab, gPanel, gDebugger;

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    testAnonCall();
  });
}

function testAnonCall() {
  let onCaretUpdated = waitForCaretUpdated(gPanel, 15);
  let onScopes = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
  callInTab(gTab, "evalCall");
  promise.all([onCaretUpdated, onScopes]).then(() => {
    ok(isCaretPos(gPanel, 15),
      "The source editor caret position was incorrect.");
    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused.");
    is(gDebugger.document.querySelectorAll(".dbg-stackframe").length, 3,
      "Should have three frames.");
    is(gDebugger.document.querySelector("#stackframe-0 .dbg-stackframe-title").getAttribute("value"),
      "anonFunc", "Frame name should be 'anonFunc'.");

    testInferredName();
  });
}

function testInferredName() {
  waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES).then(() => {
    ok(isCaretPos(gPanel, 15),
      "The source editor caret position was incorrect.");
    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused.");
    is(gDebugger.document.querySelectorAll(".dbg-stackframe").length, 3,
      "Should have three frames.");
    is(gDebugger.document.querySelector("#stackframe-0 .dbg-stackframe-title").getAttribute("value"),
      "a/<", "Frame name should be 'a/<'.");

    resumeDebuggerThenCloseAndFinish(gPanel);
  });

  gDebugger.gThreadClient.resume();
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
});
