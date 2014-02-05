/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 737803: Setting a breakpoint in a line without code should move
 * the icon to the actual location.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gDebuggee, gPanel, gDebugger;
  let gEditor, gSources, gBreakpoints, gBreakpointsAdded, gBreakpointsRemoving;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gBreakpointsAdded = gBreakpoints._added;
    gBreakpointsRemoving = gBreakpoints._removing;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1).then(performTest);
    gDebuggee.firstCall();
  });

  function performTest() {
    is(gBreakpointsAdded.size, 0,
      "No breakpoints currently added.");
    is(gBreakpointsRemoving.size, 0,
      "No breakpoints currently being removed.");
    is(gEditor.getBreakpoints().length, 0,
      "No breakpoints currently shown in the editor.");

    gEditor.on("breakpointAdded", onEditorBreakpointAdd);
    gPanel.addBreakpoint({ url: gSources.selectedValue, line: 4 }).then(onBreakpointAdd);
  }

  let onBpDebuggerAdd = false;
  let onBpEditorAdd = false;

  function onBreakpointAdd(aBreakpointClient) {
    ok(aBreakpointClient,
      "Breakpoint added, client received.");
    is(aBreakpointClient.location.url, gSources.selectedValue,
      "Breakpoint client url is the same.");
    is(aBreakpointClient.location.line, 6,
      "Breakpoint client line is new.");

    is(aBreakpointClient.requestedLocation.url, gSources.selectedValue,
      "Requested location url is correct");
    is(aBreakpointClient.requestedLocation.line, 4,
      "Requested location line is correct");

    onBpDebuggerAdd = true;
    maybeFinish();
  }

  function onEditorBreakpointAdd() {
    gEditor.off("breakpointAdded", onEditorBreakpointAdd);

    is(gEditor.getBreakpoints().length, 1,
      "There is only one breakpoint in the editor");

    ok(!gBreakpoints._getAdded({ url: gSources.selectedValue, line: 4 }),
      "There isn't any breakpoint added on an invalid line.");
    ok(!gBreakpoints._getRemoving({ url: gSources.selectedValue, line: 4 }),
      "There isn't any breakpoint removed from an invalid line.");

    ok(gBreakpoints._getAdded({ url: gSources.selectedValue, line: 6 }),
      "There is a breakpoint added on the actual line.");
    ok(!gBreakpoints._getRemoving({ url: gSources.selectedValue, line: 6 }),
      "There isn't any breakpoint removed from the actual line.");

    gBreakpoints._getAdded({ url: gSources.selectedValue, line: 6 }).then(aBreakpointClient => {
      is(aBreakpointClient.location.url, gSources.selectedValue,
        "Breakpoint client location url is correct.");
      is(aBreakpointClient.location.line, 6,
        "Breakpoint client location line is correct.");

      onBpEditorAdd = true;
      maybeFinish();
    });
  }

  function maybeFinish() {
    info("onBpDebuggerAdd: " + onBpDebuggerAdd);
    info("onBpEditorAdd: " + onBpEditorAdd);

    if (onBpDebuggerAdd && onBpEditorAdd) {
      resumeDebuggerThenCloseAndFinish(gPanel);
    }
  }
}
