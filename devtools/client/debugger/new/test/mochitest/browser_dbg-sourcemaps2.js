/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests loading sourcemapped sources, setting breakpoints, and
// stepping in them.

// This source map does not have source contents, so it's fetched separately

add_task(function* () {
  const dbg = yield initDebugger("doc-sourcemaps2.html");
  const { selectors: { getBreakpoint, getBreakpoints }, getState } = dbg;

  yield waitForSources(dbg, "main.js", "main.min.js");

  ok(true, "Original sources exist");
  const mainSrc = findSource(dbg, "main.js");

  yield selectSource(dbg, mainSrc);

  // Test that breakpoint is not off by a line.
  yield addBreakpoint(dbg, mainSrc, 4);
  is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
  ok(getBreakpoint(getState(), { sourceId: mainSrc.id, line: 4 }),
     "Breakpoint has correct line");

  invokeInTab("logMessage");

  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "main.js", 4);
});
