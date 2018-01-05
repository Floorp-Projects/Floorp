/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the breakpoints are hit in various situations.

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  const { selectors: { getSelectedSource }, getState } = dbg;

  // Make sure we can set a top-level breakpoint and it will be hit on
  // reload.
  await addBreakpoint(dbg, "scripts.html", 18);
  reload(dbg);

  await waitForDispatch(dbg, "NAVIGATE");
  await waitForSelectedSource(dbg, "doc-scripts.html");
  await waitForPaused(dbg);

  assertPausedLocation(dbg);
  await resume(dbg);

  // Create an eval script that pauses itself.
  invokeInTab("doEval");
  await waitForPaused(dbg);

  await resume(dbg);
  const source = getSelectedSource(getState()).toJS();
  ok(!source.url, "It is an eval source");

  await addBreakpoint(dbg, source, 5);
  invokeInTab("evaledFunc");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
});
