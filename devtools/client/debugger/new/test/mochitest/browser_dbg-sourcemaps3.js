/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests loading sourcemapped sources, setting breakpoints, and
// inspecting restored scopes.

function toggleNode(dbg, index) {
  clickElement(dbg, "scopeNode", index);
}

function getLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}

function hasScopeNode(dbg, index) {
  return !!findElement(dbg, "scopeNode", index);
}

async function waitForScopeNode(dbg, index) {
  const selector = getSelector("scopeNode", index);
  return waitForElement(dbg, selector);
}

// This source map does not have source contents, so it's fetched separately
add_task(async function() {
  // NOTE: the CORS call makes the test run times inconsistent
  requestLongerTimeout(2);

  const dbg = await initDebugger("doc-sourcemaps3.html");
  const { selectors: { getBreakpoint, getBreakpoints }, getState } = dbg;

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

  await waitForDispatch(dbg, "MAP_SCOPES");

  is(getLabel(dbg, 1), "Block");
  is(getLabel(dbg, 2), "<this>");
  is(getLabel(dbg, 3), "na");
  is(getLabel(dbg, 4), "nb");

  is(getLabel(dbg, 5), "Block");
  is(
    hasScopeNode(dbg, 8) && !hasScopeNode(dbg, 9),
    true,
    "scope count before expand"
  );
  toggleNode(dbg, 5);

  await waitForScopeNode(dbg, 9);

  is(getLabel(dbg, 6), "ma");
  is(getLabel(dbg, 7), "mb");

  is(
    hasScopeNode(dbg, 10) && !hasScopeNode(dbg, 11),
    true,
    "scope count before expand"
  );
  toggleNode(dbg, 8);

  await waitForScopeNode(dbg, 11);

  is(getLabel(dbg, 9), "a");
  is(getLabel(dbg, 10), "arguments");
  is(getLabel(dbg, 11), "b");
});
