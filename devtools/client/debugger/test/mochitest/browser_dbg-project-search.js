/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Testing various project search features

"use strict";

add_task(async function testOpeningAndClosingEmptyProjectSearch() {
  const dbg = await initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );
  await openProjectSearch(dbg);
  await closeProjectSearch(dbg);
});

add_task(async function testProjectSearchCloseOnNavigation() {
  const dbg = await initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );

  await selectSource(dbg, "script-switching-01.js");

  await openProjectSearch(dbg);
  await doProjectSearch(dbg, "first");

  is(dbg.selectors.getActiveSearch(), "project");

  await navigate(dbg, "doc-scripts.html");

  // wait for the project search to close
  await waitForState(dbg, state => !dbg.selectors.getActiveSearch());
});

add_task(async function testSimpleProjectSearch() {
  const dbg = await initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );

  await selectSource(dbg, "script-switching-01.js");

  await openProjectSearch(dbg);

  const searchTerm = "first";

  await doProjectSearch(dbg, searchTerm);

  const queryMatch = findElement(dbg, "fileMatch").querySelector(
    ".query-match"
  );
  is(
    queryMatch.innerText,
    searchTerm,
    "The highlighted text matches the search term"
  );

  info("Select a result match to open the location in the source");
  await selectResultMatch(dbg);
  await waitForLoadedSource(dbg, "script-switching-01.js");

  is(dbg.selectors.getActiveSearch(), null);

  ok(
    dbg.selectors.getSelectedSource().url.includes("script-switching-01.js"),
    "The correct source (script-switching-01.js) is selected"
  );
});

// Test expanding search results to reveal the search matches.
add_task(async function testExpandSearchResultsToShowMatches() {
  const dbg = await initDebugger("doc-react.html", "App.js");

  await openProjectSearch(dbg);
  await doProjectSearch(dbg, "we");

  is(getExpandedResultsCount(dbg), 159);

  const collapsedNodes = findAllElements(dbg, "projectSearchCollapsed");
  is(collapsedNodes.length, 1);

  collapsedNodes[0].click();

  is(getExpandedResultsCount(dbg), 367);
});

async function selectResultMatch(dbg) {
  const select = waitForState(dbg, () => !dbg.selectors.getActiveSearch());
  await clickElement(dbg, "fileMatch");
  return select;
}
