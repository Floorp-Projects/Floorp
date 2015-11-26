/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Breaking in the middle of a script evaluated by the console should
 * work
 */

function test() {
  Task.spawn(runTests);
}

function* runTests() {
  let TAB_URL = EXAMPLE_URL + "doc_empty-tab-01.html";
  let [,, panel] = yield initDebugger(TAB_URL);
  let dbgWin = panel.panelWin;
  let sources = dbgWin.DebuggerView.Sources;
  let frames = dbgWin.DebuggerView.StackFrames;
  let editor = dbgWin.DebuggerView.editor;
  let toolbox = gDevTools.getToolbox(panel.target);

  let paused = promise.all([
    waitForEditorEvents(panel, "cursorActivity"),
    waitForDebuggerEvents(panel, dbgWin.EVENTS.SOURCE_SHOWN)
  ]);

  toolbox.once("webconsole-ready", () => {
    ok(toolbox.splitConsole, "Split console is shown.");
    let jsterm = toolbox.getPanel("webconsole").hud.jsterm;
    jsterm.execute('debugger');
  });
  EventUtils.synthesizeKey("VK_ESCAPE", {}, dbgWin);

  yield paused;
  is(sources.selectedItem.attachment.label, 'SCRIPT0',
     'Anonymous source is selected in sources');
  dump('text ::' + editor.getText() + '::\n');
  ok(editor.getText() === 'debugger', 'Editor has correct text');

  yield toolbox.closeSplitConsole();
  yield resumeDebuggerThenCloseAndFinish(panel);
}
