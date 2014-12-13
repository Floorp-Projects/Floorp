/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the performance tool can jump to the debugger.
 */

function spawnTest () {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { viewSourceInDebugger } = panel.panelWin;

  yield viewSourceInDebugger(SIMPLE_URL, 14);

  let debuggerPanel = toolbox.getPanel("jsdebugger");
  ok(debuggerPanel, "The debugger panel was opened.");

  let { DebuggerView } = debuggerPanel.panelWin;
  let Sources = DebuggerView.Sources;

  is(Sources.selectedValue, getSourceActor(Sources, SIMPLE_URL),
    "The correct source is shown in the debugger.");
  is(DebuggerView.editor.getCursor().line + 1, 14,
    "The correct line is highlighted in the debugger's source editor.");

  yield teardown(panel);
  finish();
}
