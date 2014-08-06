/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler can jump to the debugger.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let toolbox = panel._toolbox;
  let EVENTS = panel.panelWin.EVENTS;

  let whenSourceShown = panel.panelWin.once(EVENTS.SOURCE_SHOWN_IN_JS_DEBUGGER);
  panel.panelWin.viewSourceInDebugger(SIMPLE_URL, 14);
  yield whenSourceShown;

  let debuggerPanel = toolbox.getPanel("jsdebugger");
  ok(debuggerPanel, "The debugger panel was opened.");

  let { DebuggerView } = debuggerPanel.panelWin;

  is(DebuggerView.Sources.selectedValue, SIMPLE_URL,
    "The correct source is shown in the debugger.");
  is(DebuggerView.editor.getCursor().line + 1, 14,
    "The correct line is highlighted in the debugger's source editor.");

  yield teardown(panel);
  finish();
});
