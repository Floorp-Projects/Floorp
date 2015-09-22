/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that anonymous functions appear in the stack frame list with either
 * their displayName property or a SpiderMonkey-inferred name.
 */

const TAB_URL = EXAMPLE_URL + "doc_function-display-name.html";

var gTab, gPanel, gDebugger;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    testAnonCall();
  });
}

function testAnonCall() {
  waitForSourceAndCaretAndScopes(gPanel, ".html", 15).then(() => {
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

  callInTab(gTab, "evalCall");
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

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
});
