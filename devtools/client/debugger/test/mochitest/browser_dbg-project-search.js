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

add_task(async function testMatchesForRegexSearches() {
  const dbg = await initDebugger("doc-react.html", "App.js");
  await openProjectSearch(dbg);

  type(dbg, "import .* from 'react'");
  await clickElement(dbg, "projectSearchModifiersRegexMatch");

  await waitForSearchResults(dbg, 2);

  const queryMatch = findAllElements(dbg, "fileMatch")[1].querySelector(
    ".query-match"
  );

  is(
    queryMatch.innerText,
    "import React, { Component } from 'react'",
    "The highlighted text matches the search term"
  );

  // Turn off the regex modifier so does not break tests below
  await clickElement(dbg, "projectSearchModifiersRegexMatch");
  await closeProjectSearch(dbg);
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

add_task(async function testSearchModifiers() {
  const dbg = await initDebugger("doc-react.html", "App.js");

  await assertProjectSearchModifier(
    dbg,
    "projectSearchModifiersCaseSensitive",
    "FIELDS",
    "case sensitive",
    { resultWithModifierOn: 0, resultWithModifierOff: 2 }
  );
  await assertProjectSearchModifier(
    dbg,
    "projectSearchModifiersRegexMatch",
    `\\*`,
    "regex match",
    { resultWithModifierOn: 12, resultWithModifierOff: 0 }
  );
  await assertProjectSearchModifier(
    dbg,
    "projectSearchModifiersWholeWordMatch",
    "so",
    "whole word match",
    { resultWithModifierOn: 6, resultWithModifierOff: 16 }
  );
});

async function assertProjectSearchModifier(
  dbg,
  searchModifierBtn,
  searchTerm,
  title,
  expected
) {
  info(`Assert ${title} search modifier`);
  await openProjectSearch(dbg);
  type(dbg, searchTerm);
  info(`Turn on the ${title} search modifier option`);
  await clickElement(dbg, searchModifierBtn);

  let results = await waitForSearchResults(dbg, expected.resultWithModifierOn);
  is(
    results.length,
    expected.resultWithModifierOn,
    `${results.length} results where found`
  );

  info(`Turn off the ${title} search modifier`);
  await clickElement(dbg, searchModifierBtn);

  results = await waitForSearchResults(dbg, expected.resultWithModifierOff);
  is(
    results.length,
    expected.resultWithModifierOff,
    `${results.length} results where found`
  );

  await closeProjectSearch(dbg);
}

async function selectResultMatch(dbg) {
  const select = waitForState(dbg, () => !dbg.selectors.getActiveSearch());
  await clickElement(dbg, "fileMatch");
  return select;
}
