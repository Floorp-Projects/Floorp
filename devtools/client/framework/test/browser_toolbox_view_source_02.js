/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that Toolbox#viewSourceInDebugger works when debugger is already loaded.
 */

var URL = `${URL_ROOT}doc_viewsource.html`;
var JS_URL = `${URL_ROOT}code_math.js`;

async function viewSource() {
  const toolbox = await openNewTabAndToolbox(URL);
  await toolbox.selectTool("jsdebugger");

  await toolbox.viewSourceInDebugger(JS_URL, 2);

  const debuggerPanel = toolbox.getPanel("jsdebugger");
  ok(debuggerPanel, "The debugger panel was opened.");
  is(toolbox.currentToolId, "jsdebugger", "The debugger panel was selected.");

  assertSelectedLocationInDebugger(debuggerPanel, 2, undefined);

  await closeToolboxAndTab(toolbox);
  finish();
}

function test() {
  viewSource().then(finish, (aError) => {
    ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
    finish();
  });
}
