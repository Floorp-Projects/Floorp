/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the breakpoints are hit in various situations.

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  const {
    selectors: { getSelectedSource },
    getState,
  } = dbg;

  await selectSource(dbg, "scripts.html");

  // Make sure we can set a top-level breakpoint and it will be hit on
  // reload.
  await addBreakpoint(dbg, "scripts.html", 21);

  reload(dbg);

  await waitForDispatch(dbg.store, "NAVIGATE");
  await waitForSelectedSource(dbg, "doc-scripts.html");
  await waitForPaused(dbg);

  assertPausedLocation(dbg);
  await resume(dbg);

  info("Create an eval script that pauses itself.");
  invokeInTab("doEval");
  await waitForPaused(dbg);

  await resume(dbg);
  const source = getSelectedSource();
  ok(!source.url, "It is an eval source");

  await addBreakpoint(dbg, source, 5);
  invokeInTab("evaledFunc");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
});
