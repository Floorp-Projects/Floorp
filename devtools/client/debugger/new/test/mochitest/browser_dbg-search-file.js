/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the search bar correctly responds to queries, enter, shift enter

function waitForSearchState(dbg) {
  return waitForState(dbg, () => getCM(dbg).state.search);
}

function getFocusedEl(dbg) {
  let doc = dbg.win.document;
  return doc.activeElement;
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  const {
    selectors: { getBreakpoints, getBreakpoint, getActiveSearch },
    getState
  } = dbg;
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source.url);

  const cm = getCM(dbg);
  pressKey(dbg, "fileSearch");
  is(dbg.selectors.getActiveSearch(dbg.getState()), "file");

  // test closing and re-opening
  pressKey(dbg, "Escape");
  is(dbg.selectors.getActiveSearch(dbg.getState()), null);

  pressKey(dbg, "fileSearch");

  const el = getFocusedEl(dbg);

  type(dbg, "con");
  await waitForSearchState(dbg);

  const state = cm.state.search;

  pressKey(dbg, "Enter");
  is(state.posFrom.line, 3);

  pressKey(dbg, "Enter");
  is(state.posFrom.line, 4);

  pressKey(dbg, "ShiftEnter");
  is(state.posFrom.line, 3);

  pressKey(dbg, "fileSearch");
  type(dbg, "fun");

  pressKey(dbg, "Enter");
  is(state.posFrom.line, 4);

  // selecting another source keeps search open
  await selectSource(dbg, "simple2");
  ok(findElement(dbg, "searchField"), "Search field is still visible");
});
