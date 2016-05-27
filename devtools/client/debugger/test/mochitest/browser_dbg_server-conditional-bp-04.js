/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that conditional breakpoints with undefined expressions
 * maintain their conditions when re-enabling them (with
 * server-side support)
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require("./content/queries");
    const constants = gDebugger.require("./content/constants");
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    Task.spawn(function* () {
      let onCaretUpdated = waitForCaretAndScopes(gPanel, 17);
      callInTab(gTab, "ermahgerd");
      yield onCaretUpdated;

      const location = { actor: gSources.selectedValue, line: 18 };

      yield actions.addBreakpoint(location, "");
      yield actions.disableBreakpoint(location);
      yield actions.addBreakpoint(location);

      const bp = queries.getBreakpoint(getState(), location);
      is(bp.condition, "", "The conditional expression is correct.");

      resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });
}
