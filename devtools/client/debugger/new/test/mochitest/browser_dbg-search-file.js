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

add_task(function*() {
  const dbg = yield initDebugger("doc-scripts.html");
  const {
    selectors: { getBreakpoints, getBreakpoint, getActiveSearch },
    getState
  } = dbg;
  const source = findSource(dbg, "simple1.js");

  yield selectSource(dbg, source.url);

  const cm = getCM(dbg);
  pressKey(dbg, "fileSearch");
  is(dbg.selectors.getActiveSearchState(dbg.getState()), "file");

  // test closing and re-opening
  pressKey(dbg, "Escape");
  is(dbg.selectors.getActiveSearchState(dbg.getState()), null);

  pressKey(dbg, "fileSearch");

  const el = getFocusedEl(dbg);

  type(dbg, "con");
  yield waitForSearchState(dbg);

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
  yield selectSource(dbg, "simple2");
  pressKey(dbg, "Enter");
  is(state.posFrom.line, 0);
});
