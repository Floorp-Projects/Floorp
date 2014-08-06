/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler can jump to the debugger, when the source was already
 * loaded in that tool.
 */

let test = Task.async(function*() {
  let [target, debuggee, debuggerPanel] = yield initFrontend(SIMPLE_URL, "jsdebugger");
  let toolbox = debuggerPanel._toolbox;
  let debuggerWin = debuggerPanel.panelWin;
  let debuggerEvents = debuggerWin.EVENTS;
  let { DebuggerView } = debuggerWin;

  yield debuggerWin.once(debuggerEvents.SOURCE_SHOWN);
  ok("A source was shown in the debugger.");

  is(DebuggerView.Sources.selectedValue, SIMPLE_URL,
    "The correct source is initially shown in the debugger.");
  is(DebuggerView.editor.getCursor().line, 0,
    "The correct line is initially highlighted in the debugger's source editor.");

  yield toolbox.selectTool("jsprofiler");
  let profilerPanel = toolbox.getCurrentPanel();
  let profilerWin = profilerPanel.panelWin;
  let profilerEvents = profilerWin.EVENTS;

  let whenSourceShown = profilerWin.once(profilerEvents.SOURCE_SHOWN_IN_JS_DEBUGGER);
  profilerWin.viewSourceInDebugger(SIMPLE_URL, 14);
  yield whenSourceShown;

  let debuggerPanel = toolbox.getPanel("jsdebugger");
  ok(debuggerPanel, "The debugger panel was reselected.");

  is(DebuggerView.Sources.selectedValue, SIMPLE_URL,
    "The correct source is still shown in the debugger.");
  is(DebuggerView.editor.getCursor().line + 1, 14,
    "The correct line is now highlighted in the debugger's source editor.");

  yield teardown(profilerPanel);
  finish();
});
