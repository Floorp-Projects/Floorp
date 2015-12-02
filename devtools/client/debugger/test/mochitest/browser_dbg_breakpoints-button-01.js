/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if the breakpoints toggle button works as advertised.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    function checkBreakpointsDisabled(isDisabled, total = 3) {
      let breakpoints = gDebugger.queries.getBreakpoints(getState());

      is(breakpoints.length, total,
         "Breakpoints should still be set.");
      is(breakpoints.filter(bp => bp.disabled === isDisabled).length, total,
         "Breakpoints should be " + (isDisabled ? "disabled" : "enabled") + ".");
    }

    Task.spawn(function*() {
      yield waitForSourceShown(gPanel, "-01.js");

      yield actions.addBreakpoint({ actor: gSources.values[0], line: 5 });
      yield actions.addBreakpoint({ actor: gSources.values[1], line: 6 });
      yield actions.addBreakpoint({ actor: gSources.values[1], line: 7 });
      yield ensureThreadClientState(gPanel, "resumed");

      gSources.toggleBreakpoints();
      yield waitForDispatch(gPanel, gDebugger.constants.REMOVE_BREAKPOINT, 3);
      checkBreakpointsDisabled(true)

      const finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_ADDED, 3);
      gSources.toggleBreakpoints();
      yield waitForDispatch(gPanel, gDebugger.constants.ADD_BREAKPOINT, 3);
      checkBreakpointsDisabled(false);

      yield ensureThreadClientState(gPanel, "resumed");
      closeDebuggerAndFinish(gPanel)
    });
  });
}
