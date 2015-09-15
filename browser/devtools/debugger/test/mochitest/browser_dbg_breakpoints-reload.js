/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that setting a breakpoint on code that gets run on load, will get
 * hit when we reload.
 */

const TAB_URL = EXAMPLE_URL + "doc_breakpoints-reload.html";

var test = Task.async(function* () {
  requestLongerTimeout(4);

  const [tab,, panel] = yield initDebugger(TAB_URL);

  yield ensureSourceIs(panel, "doc_breakpoints-reload.html", true);

  const sources = panel.panelWin.DebuggerView.Sources;
  yield panel.addBreakpoint({
    actor: sources.selectedValue,
    line: 10 // "break on me" string
  });

  const paused = waitForThreadEvents(panel, "paused");
  reloadActiveTab(panel);
  const packet = yield paused;

  is(packet.why.type, "breakpoint",
     "Should have hit the breakpoint after the reload");
  is(packet.frame.where.line, 10,
     "Should have stopped at line 10, where we set the breakpoint");

  yield waitForDebuggerEvents(panel, panel.panelWin.EVENTS.SOURCE_SHOWN)
  yield resumeDebuggerThenCloseAndFinish(panel);
});
