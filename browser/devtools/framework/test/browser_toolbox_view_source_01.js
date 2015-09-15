/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that Toolbox#viewSourceInDebugger works when debugger is not
 * yet opened.
 */

var URL = `${URL_ROOT}doc_viewsource.html`;
var JS_URL = `${URL_ROOT}code_math.js`;

function *viewSource() {
  let toolbox = yield loadToolbox(URL);

  yield toolbox.viewSourceInDebugger(JS_URL, 2);

  let debuggerPanel = toolbox.getPanel("jsdebugger");
  ok(debuggerPanel, "The debugger panel was opened.");
  is(toolbox.currentToolId, "jsdebugger", "The debugger panel was selected.");

  let { DebuggerView } = debuggerPanel.panelWin;
  let Sources = DebuggerView.Sources;

  is(Sources.selectedValue, getSourceActor(Sources, JS_URL),
    "The correct source is shown in the debugger.");
  is(DebuggerView.editor.getCursor().line + 1, 2,
    "The correct line is highlighted in the debugger's source editor.");

  yield unloadToolbox(toolbox);
  finish();
}

function test () {
  Task.spawn(viewSource).then(finish, (aError) => {
    ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
    finish();
  });
}
