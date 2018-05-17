/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that `debugger` statements are hit before the debugger even
// initializes and it properly highlights the right location in the
// debugger.

async function waitOnToolbox(toolbox, event) {
  return new Promise(resolve => toolbox.on(event, resolve));
}

add_task(async function() {
  const url = EXAMPLE_URL + "doc-script-switching.html";
  const toolbox = await openNewTabAndToolbox(url, "webconsole");

  // Type "debugger" into console
  let jsterm = toolbox.getPanel("webconsole").hud.jsterm;
  jsterm.execute("debugger");

  // Wait for the debugger to be selected and make sure it's paused
  await waitOnToolbox(toolbox, "jsdebugger-selected");
  is(toolbox.threadClient.state, "paused");

  // Create a dbg context
  const dbg = createDebuggerContext(toolbox);
  const {
    selectors: { getSelectedSource },
    getState
  } = dbg;

  // Make sure the thread is paused in the right source and location
  await waitForPaused(dbg);
  is(getCM(dbg).getValue(), "debugger");
  const source = getSelectedSource(getState()).toJS();
  assertPausedLocation(dbg);
});
