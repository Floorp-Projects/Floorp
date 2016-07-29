/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Hitting ESC to open the split console when paused on reload should not stop
 * the pending navigation.
 */

function test() {
  Task.spawn(runTests);
}

function* runTests() {
  let TAB_URL = EXAMPLE_URL + "doc_split-console-paused-reload.html";
  let options = {
    source: TAB_URL,
    line: 1
  };
  let [,, panel] = yield initDebugger(TAB_URL, options);
  let dbgWin = panel.panelWin;
  let sources = dbgWin.DebuggerView.Sources;
  let frames = dbgWin.DebuggerView.StackFrames;
  let toolbox = gDevTools.getToolbox(panel.target);

  yield panel.addBreakpoint({ actor: getSourceActor(sources, TAB_URL), line: 16 });
  info("Breakpoint was set.");
  dbgWin.DebuggerController._target.activeTab.reload();
  info("Page reloaded.");
  yield waitForSourceAndCaretAndScopes(panel, ".html", 16);
  yield ensureThreadClientState(panel, "paused");
  info("Breakpoint was hit.");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    frames.selectedItem.target,
    dbgWin);
  info("The breadcrumb received focus.");

  // This is the meat of the test.
  let jsterm = yield getSplitConsole(toolbox);

  is(dbgWin.gThreadClient.state, "paused", "Execution is still paused.");

  let dbgFrameConsoleEvalResult = yield jsterm.execute("privateVar");

  is(
    dbgFrameConsoleEvalResult.querySelector(".console-string").textContent,
    '"privateVarValue"',
    "Got the expected split console result on paused debugger"
  );

  yield dbgWin.gThreadClient.resume();

  is(dbgWin.gThreadClient.state, "attached", "Execution is resumed.");

  // Get the last evaluation result adopted by the new debugger.
  let mainTargetConsoleEvalResult = yield jsterm.execute("$_");

  is(
    mainTargetConsoleEvalResult.querySelector(".console-string").textContent,
    '"privateVarValue"',
    "Got the expected split console log on $_ executed on resumed debugger"
  );

  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
  yield closeDebuggerAndFinish(panel);
}
