/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the search bar correctly responds to queries, enter, shift enter

function waitForSearchState(dbg) {
  return waitForState(dbg, () => getCM(dbg).state.search);
}

function getFocusedEl(dbg) {
  const doc = dbg.win.document;
  return doc.activeElement;
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const {
    selectors: { getBreakpoints, getBreakpoint, getActiveSearch },
    getState
  } = dbg;
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source.url);

  const cm = getCM(dbg);
  pressKey(dbg, "fileSearch");
  is(dbg.selectors.getActiveSearch(), "file");

  // test closing and re-opening
  pressKey(dbg, "Escape");
  is(dbg.selectors.getActiveSearch(), null);

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

  pressKey(dbg, "fileSearchNext");
  is(state.posFrom.line, 4);

  pressKey(dbg, "fileSearchPrev");
  is(state.posFrom.line, 3);

  pressKey(dbg, "fileSearch");
  type(dbg, "fun");

  pressKey(dbg, "Enter");
  is(state.posFrom.line, 4);

  // selecting another source keeps search open
  await selectSource(dbg, "simple2");
  ok(findElement(dbg, "searchField"), "Search field is still visible");
});
