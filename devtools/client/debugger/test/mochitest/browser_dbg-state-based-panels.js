/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that breakpoint panels open when their relevant breakpoint is hit

function getPaneElements(dbg) {
  return findElementWithSelector(dbg, '.breakpoints-pane').childNodes;
}

add_task(async function() {
  const dbg = await initDebugger("doc-sources.html", "simple1");

  clickElementWithSelector(dbg, ".breakpoints-pane h2");

  info("Confirm the breakpoints pane is closed");
  is(getPaneElements(dbg).length, 1);

  await selectSource(dbg, "simple1.js");
  await addBreakpoint(dbg, "simple1.js", 4);

  info("Trigger the breakpoint ane ensure we're paused");
  invokeInTab("main");
  await waitForPaused(dbg, "simple1.js");

  info("Confirm the breakpoints pane is open");
  is(getPaneElements(dbg).length, 2);

  await resume(dbg);

  info("Confirm the breakpoints pane is closed again");
  is(getPaneElements(dbg).length, 1);
});
