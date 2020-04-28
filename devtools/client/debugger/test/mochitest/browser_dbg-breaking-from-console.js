/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that `debugger` statements are hit before the debugger even
// initializes and it properly highlights the right location in the
// debugger.

add_task(async function() {
  const url = EXAMPLE_URL + "doc-script-switching.html";
  const toolbox = await openNewTabAndToolbox(url, "webconsole");

  // Type "debugger" into console
  let wrapper = toolbox.getPanel("webconsole").hud.ui.wrapper;
  wrapper.dispatchEvaluateExpression("debugger");

  // Wait for the debugger to be selected and make sure it's paused
  await waitOnToolbox(toolbox, "jsdebugger-selected");
  is(toolbox.threadFront.state, "paused");

  // Create a dbg context
  const dbg = createDebuggerContext(toolbox);

  // Make sure the thread is paused in the right source and location
  await waitForPaused(dbg);
  is(getCM(dbg).getValue(), "debugger");
  assertPausedLocation(dbg);
});

async function waitOnToolbox(toolbox, event) {
  return new Promise(resolve => toolbox.on(event, resolve));
}
