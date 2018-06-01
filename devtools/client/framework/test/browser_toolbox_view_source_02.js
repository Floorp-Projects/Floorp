/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that Toolbox#viewSourceInDebugger works when debugger is already loaded.
 */

var URL = `${URL_ROOT}doc_viewsource.html`;
var JS_URL = `${URL_ROOT}code_math.js`;

// Force the old debugger UI since it's directly used (see Bug 1301705)
Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", false);
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.new-debugger-frontend");
});

async function viewSource() {
  const toolbox = await openNewTabAndToolbox(URL);
  const { panelWin: debuggerWin } = await toolbox.selectTool("jsdebugger");
  const debuggerEvents = debuggerWin.EVENTS;
  const { DebuggerView } = debuggerWin;
  const Sources = DebuggerView.Sources;

  await debuggerWin.once(debuggerEvents.SOURCE_SHOWN);
  ok("A source was shown in the debugger.");

  is(Sources.selectedValue, getSourceActor(Sources, JS_URL),
    "The correct source is initially shown in the debugger.");
  is(DebuggerView.editor.getCursor().line, 0,
    "The correct line is initially highlighted in the debugger's source editor.");

  await toolbox.viewSourceInDebugger(JS_URL, 2);

  const debuggerPanel = toolbox.getPanel("jsdebugger");
  ok(debuggerPanel, "The debugger panel was opened.");
  is(toolbox.currentToolId, "jsdebugger", "The debugger panel was selected.");

  is(Sources.selectedValue, getSourceActor(Sources, JS_URL),
    "The correct source is shown in the debugger.");
  is(DebuggerView.editor.getCursor().line + 1, 2,
    "The correct line is highlighted in the debugger's source editor.");

  await closeToolboxAndTab(toolbox);
  finish();
}

function test() {
  viewSource().then(finish, (aError) => {
    ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
    finish();
  });
}
