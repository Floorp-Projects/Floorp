/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that `debugger` statements are hit before the debugger even
// initializes and it properly highlights the right location in the
// debugger.

add_task(function* () {
  const url = EXAMPLE_URL + "doc-script-switching.html";
  const toolbox = yield openNewTabAndToolbox(url, "webconsole");

  // Type "debugger" into console
  let jsterm = toolbox.getPanel("webconsole").hud.jsterm;
  jsterm.execute("debugger");

  // Wait for the debugger to be selected and make sure it's paused
  yield new Promise((resolve) => {
    toolbox.on("jsdebugger-selected", resolve);
  });
  is(toolbox.threadClient.state, "paused");

  // Create a dbg context
  const dbg = createDebuggerContext(toolbox);
  const { selectors: { getSelectedSource }, getState } = dbg;

  // Make sure the thread is paused in the right source and location
  yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");
  is(dbg.win.cm.getValue(), "debugger");
  const source = getSelectedSource(getState()).toJS();
  assertPausedLocation(dbg, source, 1);
});
