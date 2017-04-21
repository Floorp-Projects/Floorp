/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that an error while loading a sourcemap does not break
// debugging.

add_task(function*() {
  // NOTE: the CORS call makes the test run times inconsistent
  requestLongerTimeout(2);

  const dbg = yield initDebugger("doc-sourcemap-bogus.html");
  const { selectors: { getSources }, getState } = dbg;

  yield selectSource(dbg, "bogus-map.js");

  // We should still be able to set breakpoints and pause in the
  // generated source.
  yield addBreakpoint(dbg, "bogus-map.js", 4);
  invokeInTab("runCode");
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "bogus-map.js", 4);

  // Make sure that only the single generated source exists. The
  // sourcemap failed to download.
  is(getSources(getState()).size, 1, "Only 1 source exists");
});
