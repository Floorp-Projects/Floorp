/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 737803: Setting a breakpoint in a line without code should move
 * the icon to the actual location.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const gController = gDebugger.DebuggerController;
    const constants = gDebugger.require('./content/constants');
    const queries = gDebugger.require('./content/queries');
    const actions = bindActionCreators(gPanel);

    Task.spawn(function*() {
      yield waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1);

      is(queries.getBreakpoints(gController.getState()).length, 0,
         "There are no breakpoints in the editor");

      const response = yield actions.addBreakpoint({
        actor: gSources.selectedValue, line: 4
      });

      ok(response.actualLocation, "has an actualLocation");
      is(response.actualLocation.line, 6, "moved to line 6");

      is(queries.getBreakpoints(gController.getState()).length, 1,
         "There is only one breakpoint in the editor");

      ok(!queries.getBreakpoint(gController.getState(), { actor: gSources.selectedValue, line: 4 }),
         "There isn't any breakpoint added on an invalid line.");
      ok(queries.getBreakpoint(gController.getState(), { actor: gSources.selectedValue, line: 6 }),
         "There isn't any breakpoint added on an invalid line.");

      resumeDebuggerThenCloseAndFinish(gPanel);
    })

    callInTab(gTab, "firstCall");
  });
}
