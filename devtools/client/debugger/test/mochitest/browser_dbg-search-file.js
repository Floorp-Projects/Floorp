/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the search bar correctly responds to queries, enter, shift enter

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  await selectSource(dbg, "simple1.js");

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
    // console.log
    62,
  ];

  await waitForCursorPosition(dbg, linesWithResults[0]);
  assertCursorPosition(
    dbg,
    linesWithResults[0],
    6,
    "typing in the search input moves the cursor in the source content"
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

  info("Check that changing the search term works");
  pressKey(dbg, "fileSearch");
  type(dbg, "doEval");

  await waitForCursorPosition(dbg, 9);
  assertCursorPosition(
    dbg,
    9,
    16,
    "The UI navigates to the new search results"
  );

  // selecting another source keeps search open
  await selectSource(dbg, "simple2.js");
  ok(findElement(dbg, "searchField"), "Search field is still visible");

  // search is always focused regardless of when or how it was opened
  pressKey(dbg, "fileSearch");
  await clickElement(dbg, "codeMirror");
  pressKey(dbg, "fileSearch");
  is(dbg.win.document.activeElement.tagName, "INPUT", "Search field focused");
});

async function navigateWithKey(dbg, key, expectedLine, assertionMessage) {
  pressKey(dbg, key);
  await waitForCursorPosition(dbg, expectedLine);
}
