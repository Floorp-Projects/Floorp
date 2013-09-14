/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that stackframes are added when debugger is paused in eval calls.
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

  is(gFrames.getItemAtIndex(0).value,
    "evalCall", "Oldest frame name should be correct.");
  is(gFrames.getItemAtIndex(0).description,
    TAB_URL, "Oldest frame url should be correct.");

  is(gFrames.getItemAtIndex(1).value,
    "(eval)", "Newest frame name should be correct.");
  is(gFrames.getItemAtIndex(1).description,
    TAB_URL, "Newest frame url should be correct.");

  is(gFrames.selectedIndex, 1,
    "Newest frame should be selected by default.");
  isnot(gFrames.selectedIndex, 0,
    "Oldest frame should not be selected.");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gFrames.getItemAtIndex(0).target,
    gDebugger);

  isnot(gFrames.selectedIndex, 1,
    "Newest frame should not be selected after click.");
  is(gFrames.selectedIndex, 0,
    "Oldest frame should be selected after click.");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gFrames.getItemAtIndex(1).target.querySelector(".dbg-stackframe-title"),
    gDebugger);

  is(gFrames.selectedIndex, 1,
    "Newest frame should be selected after click inside the newest frame.");
  isnot(gFrames.selectedIndex, 0,
    "Oldest frame should not be selected after click inside the newest frame.");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gFrames.getItemAtIndex(0).target.querySelector(".dbg-stackframe-details"),
    gDebugger);

  isnot(gFrames.selectedIndex, 1,
    "Newest frame should not be selected after click inside the oldest frame.");
  is(gFrames.selectedIndex, 0,
    "Oldest frame should be selected after click inside the oldest frame.");

  resumeDebuggerThenCloseAndFinish(gPanel);
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
});
