/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that setting a breakpoint in one tab, doesn't cause another tab at
 * the same source to pause at that location.
 */

const TAB_URL = EXAMPLE_URL + "doc_breakpoints-other-tabs.html";

var test = Task.async(function* () {
  const [tab1,, panel1] = yield initDebugger(TAB_URL);
  const [tab2,, panel2] = yield initDebugger(TAB_URL);
  const queries = panel1.panelWin.require('./content/queries');
  const actions = bindActionCreators(panel1);
  const getState = panel1.panelWin.DebuggerController.getState;

  yield ensureSourceIs(panel1, "code_breakpoints-other-tabs.js", true);

  const sources = panel1.panelWin.DebuggerView.Sources;

  yield actions.addBreakpoint({
    actor: queries.getSelectedSource(getState()).actor,
    line: 2
  });

  const paused = waitForThreadEvents(panel2, "paused");
  callInTab(tab2, "testCase");
  const packet = yield paused;

  is(packet.why.type, "debuggerStatement",
     "Should have stopped at the debugger statement, not the other tab's breakpoint");
  is(packet.frame.where.line, 3,
     "Should have stopped at line 3 (debugger statement), not line 2 (other tab's breakpoint)");

  yield teardown(panel1);
  yield resumeDebuggerThenCloseAndFinish(panel2);
});
