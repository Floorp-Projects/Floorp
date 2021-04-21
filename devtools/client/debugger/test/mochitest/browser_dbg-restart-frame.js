/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test checks the 'Restart frame' context menu item in the Call stack.

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple3.js");
  await selectSource(dbg, "simple3.js");

  info("Invokes function 'nestedA' and pauses in function 'nestedC'.");
  invokeInTab("nestedA");
  await waitForPaused(dbg);

  info("Right clicks frame 'nestedB' and selects 'Restart frame'.");
  const frameEls = findAllElementsWithSelector(dbg, ".pane.frames .frame");
  rightClickEl(dbg, frameEls[1]);
  await waitForContextMenu(dbg);

  selectContextMenuItem(dbg, "#node-menu-restart-frame");
  await waitForDispatch(dbg.store, "COMMAND");
  await waitForPaused(dbg);

  const pauseLine = getVisibleSelectedFrameLine(dbg);
  is(pauseLine, 13, "The debugger is paused on line 13.");

  const frames = dbg.selectors.getCurrentThreadFrames();
  is(frames.length, 2, "Callstack has two frames.");

  const selectedFrame = dbg.selectors.getVisibleSelectedFrame();
  is(selectedFrame.displayName, "nestedB", "The selected frame is 'nestedB'.");
});
