/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that setting a breakpoint on code that gets run on load, will get
 * hit when we reload.
 */

const TAB_URL = EXAMPLE_URL + "doc_breakpoints-reload.html";

var test = Task.async(function* () {
  requestLongerTimeout(4);

  const options = {
    source: TAB_URL,
    line: 1
  };
  const [tab,, panel] = yield initDebugger(TAB_URL, options);
  const actions = bindActionCreators(panel);

  const sources = panel.panelWin.DebuggerView.Sources;
  yield actions.addBreakpoint({
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

  yield waitForDebuggerEvents(panel, panel.panelWin.EVENTS.SOURCE_SHOWN);
  yield resumeDebuggerThenCloseAndFinish(panel);
});
