/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests loading sourcemapped sources, setting breakpoints, and
// inspecting restored scopes.
requestLongerTimeout(2);

// This source map does not have source contents, so it's fetched separately
add_task(async function() {
  // NOTE: the CORS call makes the test run times inconsistent
  await pushPref("devtools.debugger.features.map-scopes", true);

  const dbg = await initDebugger("doc-sourcemaps3.html");
  const {
    selectors: { getBreakpoint, getBreakpoints },
    getState
  } = dbg;

  await waitForSources(dbg, "bundle.js", "sorted.js", "test.js");

  ok(true, "Original sources exist");
  const sortedSrc = findSource(dbg, "sorted.js");

  await selectSource(dbg, sortedSrc);

  // Test that breakpoint is not off by a line.
  await addBreakpoint(dbg, sortedSrc, 9);
  is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
  ok(
    getBreakpoint(getState(), { sourceId: sortedSrc.id, line: 9, column: 4 }),
    "Breakpoint has correct line"
  );

  invokeInTab("test");

  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  is(getScopeLabel(dbg, 1), "Block");
  is(getScopeLabel(dbg, 2), "na");
  is(getScopeLabel(dbg, 3), "nb");

  is(getScopeLabel(dbg, 4), "Block");

  await toggleScopeNode(dbg, 4);

  is(getScopeLabel(dbg, 5), "ma");
  is(getScopeLabel(dbg, 6), "mb");

  await toggleScopeNode(dbg, 7);

  is(getScopeLabel(dbg, 8), "a");
  is(getScopeLabel(dbg, 9), "b");

  is(getScopeLabel(dbg, 10), "Module");

  await toggleScopeNode(dbg, 10);

  is(getScopeLabel(dbg, 11), "binaryLookup:o()");
  is(getScopeLabel(dbg, 12), "comparer:t()");
  is(getScopeLabel(dbg, 13), "fancySort");
});
