/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1008372: Setting a breakpoint in a line without code should move
 * the icon to the actual location, and if a breakpoint already exists
 * on the new location don't duplicate
 */

const TAB_URL = EXAMPLE_URL + "doc_breakpoint-move.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gEditor, gSources, gBreakpoints, gBreakpointsAdded, gBreakpointsRemoving;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gBreakpointsAdded = gBreakpoints._added;
    gBreakpointsRemoving = gBreakpoints._removing;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 1).then(performTest);
    callInTab(gTab, "ermahgerd");
  });

  function performTest() {
    is(gBreakpointsAdded.size, 0,
      "No breakpoints currently added.");
    is(gBreakpointsRemoving.size, 0,
      "No breakpoints currently being removed.");
    is(gEditor.getBreakpoints().length, 0,
      "No breakpoints currently shown in the editor.");

    Task.spawn(function*() {
      let bpClient = yield gPanel.addBreakpoint({
        actor: gSources.selectedValue,
        line: 19
      });
      yield gPanel.addBreakpoint({
        actor: gSources.selectedValue,
        line: 20
      });

      let movedBpClient = yield gPanel.addBreakpoint({
        actor: gSources.selectedValue,
        line: 17
      });

      testMovedLocation(movedBpClient);

      yield resumeAndTestBreakpoint(19);

      yield gPanel.removeBreakpoint({
        actor: gSources.selectedValue,
        line: 19
      });

      yield resumeAndTestBreakpoint(20);
      yield doResume(gPanel);

      callInTab(gTab, "ermahgerd");
      yield waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);

      yield resumeAndTestBreakpoint(20);
      resumeDebuggerThenCloseAndFinish(gPanel);
    });
  }

  function resumeAndTestBreakpoint(line) {
    return Task.spawn(function*() {
      let event = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
      doResume(gPanel);
      yield event;
      testBreakpoint(line);
    });
  };

  function testBreakpoint(line) {
    let selectedBreakpoint = gSources._selectedBreakpointItem;
    ok(selectedBreakpoint,
       "There should be a selected breakpoint on line " + line);
    is(selectedBreakpoint.attachment.line, line,
       "The breakpoint on line " + line + " was not hit");
  }

  function testMovedLocation(breakpointClient) {
    ok(breakpointClient,
      "Breakpoint added, client received.");
    is(breakpointClient.location.actor, gSources.selectedValue,
      "Breakpoint client url is the same.");
    is(breakpointClient.location.line, 19,
      "Breakpoint client line is new.");
  }
}
