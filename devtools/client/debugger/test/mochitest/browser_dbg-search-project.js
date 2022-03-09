/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Testing basic project search

"use strict";

add_task(async function() {
  const dbg = await initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );

  await selectSource(dbg, "script-switching-01.js");

  // test opening and closing
  await openProjectSearch(dbg);
  await closeProjectSearch(dbg);

  await openProjectSearch(dbg);
  type(dbg, "first");
  pressKey(dbg, "Enter");

  await waitForState(dbg, () => getResultsFiles(dbg) === 1);

  await selectResult(dbg);

  is(dbg.selectors.getActiveSearch(), null);

  const selectedSource = dbg.selectors.getSelectedSource();
  ok(selectedSource.url.includes("script-switching-01.js"));
  await waitForLoadedSource(dbg, "script-switching-01.js");
});

// Test expanding search matches to reveal the search results.
add_task(async function() {
  const dbg = await initDebugger("doc-react.html", "App.js");
  await openProjectSearch(dbg);
  type(dbg, "we");
  pressKey(dbg, "Enter");
  await waitForState(dbg, state => state.projectTextSearch.status === "DONE");

  is(getExpandedResultsCount(dbg), 18);

  const collapsedNodes = findAllElements(dbg, "projectSearchCollapsed");
  is(collapsedNodes.length, 1);

  collapsedNodes[0].click();

  is(getExpandedResultsCount(dbg), 226);
});

// Test the prioritization of source-mapped files. (Bug 1642778)
add_task(async function() {
  const dbg = await initDebugger("doc-react.html", "App.js");
  await openProjectSearch(dbg);
  type(dbg, "componentDidMount");
  pressKey(dbg, "Enter");
  await waitForState(dbg, state => state.projectTextSearch.status === "DONE");

  is(getExpandedResultsCount(dbg), 8);

  const snippets = findAllElements(dbg, "projectSearchExpandedResults");
  const files = findAllElements(dbg, "projectSearchFileResults");

  // The first item should be the original (prettified) file
  is(
    files[0].innerText.includes(
      "browser/devtools/client/debugger/test/mochitest/examples/react/build/App.js"
    ),
    true
  );
  is(snippets[0].innerText.endsWith("componentDidMount() {"), true);
});

function openProjectSearch(dbg) {
  synthesizeKeyShortcut("CmdOrCtrl+Shift+F");
  return waitForState(
    dbg,
    state => dbg.selectors.getActiveSearch() === "project"
  );
}

function closeProjectSearch(dbg) {
  pressKey(dbg, "Escape");
  return waitForState(dbg, state => !dbg.selectors.getActiveSearch());
}

async function selectResult(dbg) {
  const select = waitForState(dbg, () => !dbg.selectors.getActiveSearch());
  await clickElement(dbg, "fileMatch");
  return select;
}

function getExpandedResultsCount(dbg) {
  return findAllElements(dbg, "projectSearchExpandedResults").length;
}

function getResultsFiles(dbg) {
  const matches = dbg.selectors
    .getTextSearchResults()
    .map(file => file.matches);

  return [...matches].length;
}
