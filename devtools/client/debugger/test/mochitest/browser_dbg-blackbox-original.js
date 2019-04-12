/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This source map does not have source contents, so it's fetched separately
add_task(async function() {
  // NOTE: the CORS call makes the test run times inconsistent
  const dbg = await initDebugger("doc-sourcemaps3.html");
  dbg.actions.toggleMapScopes();

  const {
    selectors: { getBreakpoint, getBreakpointCount },
    getState
  } = dbg;

  await waitForSources(dbg, "bundle.js", "sorted.js", "test.js");

  const sortedSrc = findSource(dbg, "sorted.js");

  await selectSource(dbg, sortedSrc);

  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg, "BLACKBOX");

  // breakpoint at line 38 in sorted
  await addBreakpoint(dbg, sortedSrc, 38);
  // invoke test
  invokeInTab("test");
  // should not pause
  is(isPaused(dbg), false);

  // unblackbox
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg, "BLACKBOX");

  // click on test
  invokeInTab("test");
  // should pause
  await waitForPaused(dbg);

  ok(true, "blackbox works");
});
