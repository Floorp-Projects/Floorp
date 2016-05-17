/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that conditional breakpoints with blank expressions
 * maintain their conditions after enabling them.
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require("./content/queries");
    const constants = gDebugger.require("./content/constants");
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    // This test forces conditional breakpoints to be evaluated on the
    // client-side
    var client = gPanel.target.client;
    client.mainRoot.traits.conditionalBreakpoints = false;

    Task.spawn(function* () {
      yield waitForSourceAndCaretAndScopes(gPanel, ".html", 17);
      const location = { actor: gSources.selectedValue, line: 18 };

      yield actions.addBreakpoint(location, "");
      yield actions.disableBreakpoint(location);
      yield actions.addBreakpoint(location);

      const bp = queries.getBreakpoint(getState(), location);
      is(bp.condition, "", "The conditional expression is correct.");

      // Reset traits back to default value
      client.mainRoot.traits.conditionalBreakpoints = true;
      resumeDebuggerThenCloseAndFinish(gPanel);
    });

    callInTab(gTab, "ermahgerd");
  });
}
