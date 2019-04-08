/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that exceptions thrown while evaluating code will pause at the point the
// exception was generated when pausing on uncaught exceptions.
add_task(async function() {
  const dbg = await initDebugger("doc-eval-throw.html");
  await togglePauseOnExceptions(dbg, true, true);

  invokeInTab("evaluateException");

  await waitForPaused(dbg);

  const state = dbg.store.getState();
  const source = dbg.selectors.getSelectedSource(state);
  ok(!source.url, "Selected source should not have a URL");
});
