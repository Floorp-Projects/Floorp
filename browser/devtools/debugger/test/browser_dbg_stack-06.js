/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that selecting a stack frame loads the right source in the editor
 * pane and highlights the proper line.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources, gFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gFrames = gDebugger.DebuggerView.StackFrames;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 6).then(performTest);
    gDebuggee.firstCall();
  });
}

function performTest() {
  is(gFrames.selectedIndex, 3,
    "Newest frame should be selected by default.");
  is(gSources.selectedIndex, 1,
    "The second source is selected in the widget.");
  is(gEditor.getText().search(/firstCall/), -1,
    "The first source is not displayed.");
  is(gEditor.getText().search(/debugger/), 172,
    "The second source is displayed.")

  waitForSourceAndCaret(gPanel, "-01.js", 6).then(() => {
    is(gFrames.selectedIndex, 0,
      "Oldest frame should be selected after click.");
    is(gSources.selectedIndex, 0,
      "The first source is now selected in the widget.");
    is(gEditor.getText().search(/firstCall/), 118,
      "The first source is displayed.");
    is(gEditor.getText().search(/debugger/), -1,
      "The second source is not displayed.");

    resumeDebuggerThenCloseAndFinish(gPanel);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.querySelector("#stackframe-3"),
    gDebugger);
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gFrames = null;
});
