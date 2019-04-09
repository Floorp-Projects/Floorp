/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function firstBreakpoint(dbg) {
  return dbg.selectors.getBreakpointsList(dbg.getState())[0];
}

// tests to make sure we do not accidentally slide the breakpoint up to the first
// function with the same name in the file.
add_task(async function() {
  const dbg = await initDebugger("doc-duplicate-functions.html", "doc-duplicate-functions");
  const source = findSource(dbg, "doc-duplicate-functions");

  await selectSource(dbg, source.url);
  await addBreakpoint(dbg, source.url, 19);

  await reload(dbg, source.url);
  await waitForState(dbg, state => dbg.selectors.getBreakpointCount(state) == 1);
  is(firstBreakpoint(dbg).location.line, 19, "Breakpoint is on line 19");
});
