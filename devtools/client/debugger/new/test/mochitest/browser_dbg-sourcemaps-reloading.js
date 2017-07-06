/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function*() {
  // NOTE: the CORS call makes the test run times inconsistent
  requestLongerTimeout(2);

  const dbg = yield initDebugger("doc-sourcemaps.html");
  const { selectors: { getBreakpoint, getBreakpoints }, getState } = dbg;

  yield waitForSources(dbg, "entry.js", "output.js", "times2.js", "opts.js");
  ok(true, "Original sources exist");
  const entrySrc = findSource(dbg, "entry.js");

  yield selectSource(dbg, entrySrc);
  ok(
    dbg.win.cm.getValue().includes("window.keepMeAlive"),
    "Original source text loaded correctly"
  );

  // Test that breakpoint sliding is not attempted. The breakpoint
  // should not move anywhere.
  yield addBreakpoint(dbg, entrySrc, 13);
  is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
  ok(
    getBreakpoint(getState(), { sourceId: entrySrc.id, line: 13 }),
    "Breakpoint has correct line"
  );

  yield addBreakpoint(dbg, entrySrc, 15);
  yield disableBreakpoint(dbg, entrySrc, 15);

  // Test reloading the debugger
  yield reload(dbg, "opts.js");
  yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");

  is(getBreakpoints(getState()).size, 2, "One breakpoint exists");

  ok(
    getBreakpoint(getState(), { sourceId: entrySrc.id, line: 13 }),
    "Breakpoint has correct line"
  );

  ok(
    getBreakpoint(getState(), {
      sourceId: entrySrc.id,
      line: 15,
      disabled: true
    }),
    "Breakpoint has correct line"
  );
});
