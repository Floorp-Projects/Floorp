/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
requestLongerTimeout(2);

add_task(async function() {
  // NOTE: the CORS call makes the test run times inconsistent
  const dbg = await initDebugger("doc-sourcemaps.html");
  const {
    selectors: { getBreakpoint, getBreakpointCount }
  } = dbg;

  await waitForSources(dbg, "entry.js", "output.js", "times2.js", "opts.js");
  ok(true, "Original sources exist");
  const entrySrc = findSource(dbg, "entry.js");

  await selectSource(dbg, entrySrc);
  ok(
    getCM(dbg)
      .getValue()
      .includes("window.keepMeAlive"),
    "Original source text loaded correctly"
  );

  await addBreakpoint(dbg, entrySrc, 5);
  await addBreakpoint(dbg, entrySrc, 15, 0);
  await disableBreakpoint(dbg, entrySrc, 15, 0);

  // Test reloading the debugger
  await reload(dbg, "opts.js");
  await waitForDispatch(dbg, "LOAD_SOURCE_TEXT");

  await waitForPaused(dbg);
  await waitForDispatch(dbg, "ADD_INLINE_PREVIEW");
  assertPausedLocation(dbg);

  await waitForBreakpointCount(dbg, 2);
  is(getBreakpointCount(), 2, "Three breakpoints exist");

  ok(
    getBreakpoint({ sourceId: entrySrc.id, line: 15, column: 0 }),
    "Breakpoint has correct line"
  );

  ok(
    getBreakpoint({
      sourceId: entrySrc.id,
      line: 15,
      column: 0,
      disabled: true
    }),
    "Breakpoint has correct line"
  );
});

async function waitForBreakpointCount(dbg, count) {
  return waitForState(
    dbg,
    state => dbg.selectors.getBreakpointCount() === count
  );
}
