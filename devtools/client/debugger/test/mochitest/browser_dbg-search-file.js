/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the search bar correctly responds to queries, enter, shift enter

const IS_MAC_OSX = AppConstants.platform === "macosx";

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const {
    selectors: { getBreakpoints, getBreakpoint, getActiveSearch },
    getState,
  } = dbg;
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source.url);

  const cm = getCM(dbg);
  pressKey(dbg, "fileSearch");
  is(dbg.selectors.getActiveSearch(), "file", "The search UI was opened");

  info("Test closing and re-opening the search UI");
  pressKey(dbg, "Escape");
  is(
    dbg.selectors.getActiveSearch(),
    null,
    "The search UI was closed when hitting Escape"
  );

  pressKey(dbg, "fileSearch");
  is(dbg.selectors.getActiveSearch(), "file", "The search UI was opened again");

  info("Search for `con` in the script");
  type(dbg, "con");
  await waitForSearchState(dbg);
  await waitForDispatch(dbg.store, "UPDATE_SEARCH_RESULTS");

  const state = cm.state.search;
  // All the lines in the script that include `con`
  const linesWithResults = [
    // const func
    4,
    // const result
    5,
    // constructor (in MyClass)
    42,
    // constructor (in Klass)
    55,
  ];

  await waitFor(
    () => getCursorPositionLine(dbg) === linesWithResults[0],
    `typing in the search input did not set the search state in expected state`
  );

  is(
    getCursorPositionLine(dbg),
    linesWithResults[0],
    "typing in the search input initialize the search"
  );

  info("Check that pressing Enter navigates forward through the results");
  await navigateWithKey(
    dbg,
    "Enter",
    linesWithResults[1],
    "Enter moves forward in the search results"
  );

  await navigateWithKey(
    dbg,
    "Enter",
    linesWithResults[2],
    "Enter moves forward in the search results"
  );

  info(
    "Check that pressing Shift+Enter navigates backward through the results"
  );
  await navigateWithKey(
    dbg,
    "ShiftEnter",
    linesWithResults[1],
    "Shift+Enter moves backward in the search results"
  );

  await navigateWithKey(
    dbg,
    "ShiftEnter",
    linesWithResults[0],
    "Shift+Enter moves backward in the search results"
  );

  info(
    "Check that navigating backward goes to the last result when we were at the first one"
  );
  await navigateWithKey(
    dbg,
    "ShiftEnter",
    linesWithResults.at(-1),
    "Shift+Enter cycles back through the results"
  );

  info(
    "Check that navigating forward goes to the first result when we were at the last one"
  );
  await navigateWithKey(
    dbg,
    "Enter",
    linesWithResults[0],
    "Enter cycles forward through the results"
  );

  if (IS_MAC_OSX) {
    info(
      "cmd+G and cmdShift+G shortcut for traversing results only work for macOS"
    );
    await navigateWithKey(
      dbg,
      "fileSearchNext",
      linesWithResults[1],
      "Cmd+G moves forward in the search results"
    );

    await navigateWithKey(
      dbg,
      "fileSearchPrev",
      linesWithResults[0],
      "Cmd+Shift+G moves backward in the search results"
    );
  }

  info("Check that changing the search term works");
  pressKey(dbg, "fileSearch");
  type(dbg, "doEval");

  await waitFor(
    () => getCursorPositionLine(dbg) === 9,
    "The UI navigates to the new search results"
  );

  // selecting another source keeps search open
  await selectSource(dbg, "simple2");
  ok(findElement(dbg, "searchField"), "Search field is still visible");

  // search is always focused regardless of when or how it was opened
  pressKey(dbg, "fileSearch");
  await clickElement(dbg, "codeMirror");
  pressKey(dbg, "fileSearch");
  is(dbg.win.document.activeElement.tagName, "INPUT", "Search field focused");
});

async function navigateWithKey(dbg, key, expectedLine, assertionMessage) {
  const currentLine = getCursorPositionLine(dbg);
  pressKey(dbg, key);
  await waitFor(
    () => currentLine !== getCursorPositionLine(dbg),
    `Pressing "${key}" did not change the position`
  );

  is(getCursorPositionLine(dbg), expectedLine, assertionMessage);
}

function getCursorPositionLine(dbg) {
  const cursorPosition = findElementWithSelector(dbg, ".cursor-position");
  const { innerText } = cursorPosition;
  // Cursor position text has the following shape: (L, C)
  // where N is the line number, and C the column number
  const line = innerText.substring(1, innerText.indexOf(","));
  return parseInt(line);
}

function waitForSearchState(dbg) {
  return waitForState(dbg, () => getCM(dbg).state.search);
}

function getFocusedEl(dbg) {
  const doc = dbg.win.document;
  return doc.activeElement;
}
