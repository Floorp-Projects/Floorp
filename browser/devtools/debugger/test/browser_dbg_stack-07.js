/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that after selecting a different stack frame, resuming reselects
 * the topmost stackframe, loads the right source in the editor pane and
 * highlights the proper line.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources, gFrames, gToolbar;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gToolbar = gDebugger.DebuggerView.Toolbar;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 6).then(performTest);
    gDebuggee.firstCall();
  });
}

function performTest() {
  return Task.spawn(function() {
    yield selectBottomFrame();
    testBottomFrame(5);

    yield performStep("StepOver");
    testTopFrame(3);

    yield selectBottomFrame();
    testBottomFrame(4);

    yield performStep("StepIn");
    testTopFrame(2);

    yield selectBottomFrame();
    testBottomFrame(4);

    yield performStep("StepOut");
    testTopFrame(2);

    yield resumeDebuggerThenCloseAndFinish(gPanel);
  });

  function selectBottomFrame() {
    let updated = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
    gFrames.selectedIndex = 0;
    return updated.then(waitForTick);
  }

  function testBottomFrame(debugLocation) {
    is(gFrames.selectedIndex, 0,
      "Oldest frame should be selected after click.");
    is(gSources.selectedIndex, 0,
      "The first source is now selected in the widget.");
    is(gEditor.getText().search(/firstCall/), 118,
      "The first source is displayed.");
    is(gEditor.getText().search(/debugger/), -1,
      "The second source is not displayed.");

    is(gEditor.getDebugLocation(), debugLocation,
      "Editor debugger location is correct.");
    ok(gEditor.hasLineClass(debugLocation, "debug-line"),
      "The debugged line is highlighted appropriately.");
  }

  function performStep(type) {
    let updated = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
    gToolbar["_on" + type + "Pressed"]();
    return updated.then(waitForTick);
  }

  function testTopFrame(frameIndex) {
    is(gFrames.selectedIndex, frameIndex,
      "Topmost frame should be selected after click.");
    is(gSources.selectedIndex, 1,
      "The second source is now selected in the widget.");
    is(gEditor.getText().search(/firstCall/), -1,
      "The second source is displayed.");
    is(gEditor.getText().search(/debugger/), 172,
      "The first source is not displayed.");
  }
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gFrames = null;
  gToolbar = null;
});
