/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that stackframes are added when debugger is paused in eval calls.
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

    waitForSourceAndCaretAndScopes(gPanel, ".html", 18).then(performTest);
    gDebuggee.evalCall();
  });
}

function performTest() {
  is(gDebugger.gThreadClient.state, "paused",
    "Should only be getting stack frames while paused.");
  is(gFrames.itemCount, 2,
    "Should have two frames.");
  is(gClassicFrames.itemCount, 2,
    "Should also have only two in the mirrored view.");

  is(gFrames.getItemAtIndex(0).value,
    "evalCall", "Oldest frame name should be correct.");
  is(gFrames.getItemAtIndex(0).description,
    TAB_URL, "Oldest frame url should be correct.");
  is(gClassicFrames.getItemAtIndex(0).value,
    TAB_URL, "Oldest frame name is mirrored correctly.");

  is(gFrames.getItemAtIndex(1).value,
    "(eval)", "Newest frame name should be correct.");
  is(gFrames.getItemAtIndex(1).description,
    TAB_URL, "Newest frame url should be correct.");
  is(gClassicFrames.getItemAtIndex(1).value,
    TAB_URL, "Newest frame name is mirrored correctly.");

  is(gFrames.selectedIndex, 1,
    "Newest frame should be selected by default.");
  is(gClassicFrames.selectedIndex, 0,
    "Newest frame should be selected by default in the mirrored view.");

  isnot(gFrames.selectedIndex, 0,
    "Oldest frame should not be selected.");
  isnot(gClassicFrames.selectedIndex, 1,
    "Oldest frame should not be selected in the mirrored view.");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gFrames.getItemAtIndex(0).target,
    gDebugger);

  isnot(gFrames.selectedIndex, 1,
    "Newest frame should not be selected after click.");
  isnot(gClassicFrames.selectedIndex, 0,
    "Newest frame in the mirrored view should not be selected.");

  is(gFrames.selectedIndex, 0,
    "Oldest frame should be selected after click.");
  is(gClassicFrames.selectedIndex, 1,
    "Oldest frame in the mirrored view should be selected.");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gFrames.getItemAtIndex(1).target.querySelector(".dbg-stackframe-title"),
    gDebugger);

  is(gFrames.selectedIndex, 1,
    "Newest frame should be selected after click inside the newest frame.");
  is(gClassicFrames.selectedIndex, 0,
    "Newest frame in the mirrored view should be selected.");

  isnot(gFrames.selectedIndex, 0,
    "Oldest frame should not be selected after click inside the newest frame.");
  isnot(gClassicFrames.selectedIndex, 1,
    "Oldest frame in the mirrored view should not be selected.");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gFrames.getItemAtIndex(0).target.querySelector(".dbg-stackframe-details"),
    gDebugger);

  isnot(gFrames.selectedIndex, 1,
    "Newest frame should not be selected after click inside the oldest frame.");
  isnot(gClassicFrames.selectedIndex, 0,
    "Newest frame in the mirrored view should not be selected.");

  is(gFrames.selectedIndex, 0,
    "Oldest frame should be selected after click inside the oldest frame.");
  is(gClassicFrames.selectedIndex, 1,
    "Oldest frame in the mirrored view should be selected.");

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
