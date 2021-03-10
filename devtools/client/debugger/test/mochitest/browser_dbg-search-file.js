/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the search bar correctly responds to queries, enter, shift enter

const IS_MAC_OSX = AppConstants.platform === "macosx";

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
  await waitForDispatch(dbg.store, "UPDATE_SEARCH_RESULTS");

  const state = cm.state.search;

  pressKey(dbg, "Enter");
  is(state.posFrom.line, 3);

  pressKey(dbg, "Enter");
  is(state.posFrom.line, 4);

  pressKey(dbg, "ShiftEnter");
  is(state.posFrom.line, 3);

  if (IS_MAC_OSX) {
    info('cmd+G and cmdShift+G shortcut for traversing results only work for macOS');
    pressKey(dbg, "fileSearchNext");
    is(state.posFrom.line, 4);

    pressKey(dbg, "fileSearchPrev");
    is(state.posFrom.line, 3);
  }

  pressKey(dbg, "fileSearch");
  type(dbg, "fun");

  pressKey(dbg, "Enter");
  is(state.posFrom.line, 4);

  // selecting another source keeps search open
  await selectSource(dbg, "simple2");
  ok(findElement(dbg, "searchField"), "Search field is still visible");

  // search is always focused regardless of when or how it was opened
  pressKey(dbg, "fileSearch");
  await clickElement(dbg, "codeMirror");
  pressKey(dbg, "fileSearch");
  is(dbg.win.document.activeElement.tagName, "INPUT", "Search field focused");

});

function waitForSearchState(dbg) {
  return waitForState(dbg, () => getCM(dbg).state.search);
}

function getFocusedEl(dbg) {
  const doc = dbg.win.document;
  return doc.activeElement;
}