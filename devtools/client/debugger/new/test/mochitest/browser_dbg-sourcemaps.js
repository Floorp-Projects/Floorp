/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests loading sourcemapped sources, setting breakpoints, and
// stepping in them.

add_task(function* () {
  const dbg = yield initDebugger("doc-sourcemaps.html");
  const { selectors: { getBreakpoint, getBreakpoints }, getState } = dbg;

  yield waitForSources(dbg, "entry.js", "output.js", "times2.js", "opts.js");
  ok(true, "Original sources exist");
  const entrySrc = findSource(dbg, "entry.js");

  yield selectSource(dbg, entrySrc);
  ok(dbg.win.cm.getValue().includes("window.keepMeAlive"),
     "Original source text loaded correctly");

  // Test that breakpoint sliding is not attempted. The breakpoint
  // should not move anywhere.
  yield addBreakpoint(dbg, entrySrc, 13);
  is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
  ok(getBreakpoint(getState(), { sourceId: entrySrc.id, line: 13 }),
     "Breakpoint has correct line");

  // Test breaking on a breakpoint
  yield addBreakpoint(dbg, "entry.js", 15);
  is(getBreakpoints(getState()).size, 2, "Two breakpoints exist");
  ok(getBreakpoint(getState(), { sourceId: entrySrc.id, line: 15 }),
     "Breakpoint has correct line");

  invokeInTab("keepMeAlive");
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, entrySrc, 15);

  yield stepIn(dbg);
  assertPausedLocation(dbg, "times2.js", 2);
  yield stepOver(dbg);
  assertPausedLocation(dbg, "times2.js", 3);

  yield stepOut(dbg);
  yield stepOut(dbg);
  assertPausedLocation(dbg, "entry.js", 16);
});
