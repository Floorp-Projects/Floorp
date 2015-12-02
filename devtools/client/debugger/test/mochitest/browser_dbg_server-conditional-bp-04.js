/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that conditional breakpoints with undefined expressions
 * are stored as plain breakpoints when re-enabling them (with
 * server-side support)
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require('./content/queries');
    const constants = gDebugger.require('./content/constants');
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    Task.spawn(function*() {
      yield waitForSourceAndCaretAndScopes(gPanel, ".html", 17);
      const location = { actor: gSources.selectedValue, line: 18 };

      yield actions.addBreakpoint(location, "");
      yield actions.disableBreakpoint(location);
      yield actions.addBreakpoint(location);

      const bp = queries.getBreakpoint(getState(), location);
      is(bp.condition, undefined, "The conditional expression is correct.");

      resumeDebuggerThenCloseAndFinish(gPanel);
    });

    callInTab(gTab, "ermahgerd");
  });
}
