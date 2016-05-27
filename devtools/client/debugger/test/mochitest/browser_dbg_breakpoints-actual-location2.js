/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1008372: Setting a breakpoint in a line without code should move
 * the icon to the actual location, and if a breakpoint already exists
 * on the new location don't duplicate
 */

const TAB_URL = EXAMPLE_URL + "doc_breakpoint-move.html";

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const gController = gDebugger.DebuggerController;
    const actions = bindActionCreators(gPanel);
    const constants = gDebugger.require("./content/constants");
    const queries = gDebugger.require("./content/queries");

    function resumeAndTestBreakpoint(line) {
      return Task.spawn(function* () {
        let event = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
        doResume(gPanel);
        yield event;
        testBreakpoint(line);
      });
    }

    function testBreakpoint(line) {
      let bp = gSources._selectedBreakpoint;
      ok(bp, "There should be a selected breakpoint on line " + line);
      is(bp.location.line, line,
         "The breakpoint on line " + line + " was not hit");
    }

    Task.spawn(function* () {
      let onCaretUpdated = waitForCaretAndScopes(gPanel, 16);
      callInTab(gTab, "ermahgerd");
      yield onCaretUpdated;

      is(queries.getBreakpoints(gController.getState()).length, 0,
         "There are no breakpoints in the editor");

      yield actions.addBreakpoint({
        actor: gSources.selectedValue,
        line: 19
      });
      yield actions.addBreakpoint({
        actor: gSources.selectedValue,
        line: 20
      });

      const response = yield actions.addBreakpoint({
        actor: gSources.selectedValue,
        line: 17
      });

      is(response.actualLocation.line, 19,
         "Breakpoint client line is new.");

      yield resumeAndTestBreakpoint(19);

      yield actions.removeBreakpoint({
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
  });
}
