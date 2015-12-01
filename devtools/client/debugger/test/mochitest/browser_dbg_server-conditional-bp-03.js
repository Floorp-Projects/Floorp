/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that conditional breakpoints survive disabled breakpoints
 * (with server-side support)
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

      yield actions.addBreakpoint(location, "hello");
      yield actions.disableBreakpoint(location);
      yield actions.addBreakpoint(location);

      const bp = queries.getBreakpoint(getState(), location);
      is(bp.condition, "hello", "The conditional expression is correct.");

      const finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.CONDITIONAL_BREAKPOINT_POPUP_SHOWING);
      EventUtils.sendMouseEvent({ type: "click" },
                                gDebugger.document.querySelector(".dbg-breakpoint"),
                                gDebugger);
      yield finished;

      const textbox = gDebugger.document.getElementById("conditional-breakpoint-panel-textbox");
      is(textbox.value, "hello", "The expression is correct (2).")

      resumeDebuggerThenCloseAndFinish(gPanel);
    });

    callInTab(gTab, "ermahgerd");
  });
}
