/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests loading sourcemapped sources for Babel's compile for..of output.

add_task(async function() {
  await pushPref("devtools.debugger.features.map-scopes", true);

  const dbg = await initDebugger("doc-babel.html");
  const { selectors: { getBreakpoint, getBreakpoints }, getState } = dbg;

  await waitForSources(dbg, "fixtures/for-of/input.js");

  ok(true, "Original sources exist");
  const sortedSrc = findSource(dbg, "fixtures/for-of/input.js");

  await selectSource(dbg, sortedSrc);

  // Test that breakpoint is not off by a line.
  await addBreakpoint(dbg, sortedSrc, 5);
  is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
  ok(
    getBreakpoint(getState(), { sourceId: sortedSrc.id, line: 5, column: 4 }),
    "Breakpoint has correct line"
  );

  invokeInTab("forOf");

  await waitForPaused(dbg);

  assertPausedLocation(dbg);

  is(getScopeLabel(dbg, 1), "For");
  is(getScopeLabel(dbg, 2), "x");
  is(getScopeValue(dbg, 2), "1");

  is(getScopeLabel(dbg, 3), "forOf");

  await toggleScopeNode(dbg, 3);

  is(getScopeLabel(dbg, 4), "doThing()");

  is(getScopeLabel(dbg, 5), "Module");

  await toggleScopeNode(dbg, 5);

  is(getScopeLabel(dbg, 6), "forOf");
  is(getScopeLabel(dbg, 7), "mod");
});
