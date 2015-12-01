/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Hitting ESC to open the split console when paused on reload should not stop
 * the pending navigation.
 */

function test() {
  Task.spawn(runTests);
}

function* runTests() {
  let TAB_URL = EXAMPLE_URL + "doc_split-console-paused-reload.html";
  let [,, panel] = yield initDebugger(TAB_URL);
  let dbgWin = panel.panelWin;
  let sources = dbgWin.DebuggerView.Sources;
  let frames = dbgWin.DebuggerView.StackFrames;
  let toolbox = gDevTools.getToolbox(panel.target);

  yield waitForSourceShown(panel, ".html");
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
  let result = toolbox.once("webconsole-ready", () => {
    ok(toolbox.splitConsole, "Split console is shown.");
    is(dbgWin.gThreadClient.state, "paused", "Execution is still paused.");
    Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
  });
  EventUtils.synthesizeKey("VK_ESCAPE", {}, dbgWin);
  yield result;
  yield resumeDebuggerThenCloseAndFinish(panel);
}
