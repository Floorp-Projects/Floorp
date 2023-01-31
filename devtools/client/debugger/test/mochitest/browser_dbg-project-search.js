/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Testing basic project search

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

add_task(async function testSearchDynamicScripts() {
  const dbg = await initDebugger("doc-minified.html");

  const executeComplete = dbg.commands.scriptCommand.execute(
    `const foo = 5; debugger; console.log(foo)`
  );
  await waitForPaused(dbg);

  await openProjectSearch(dbg);
  type(dbg, "foo");
  pressKey(dbg, "Enter");

  const fileResults = await waitForSearchResults(dbg, 1);

  ok(
    /source\d+/g.test(fileResults[0].innerText),
    "The search result was found in the eval script."
  );

  await closeProjectSearch(dbg);
  await resume(dbg);
  await executeComplete;
});

// Tests that minified sources are ignored when the prettyfied versions
// exist.
add_task(async function testIgnoreMinifiedSourceForPrettySource() {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await openProjectSearch(dbg);
  let fileResults = await doProjectSearch(dbg, "stuff");

  is(fileResults.length, 1, "Only the result was found");
  ok(
    fileResults[0].innerText.includes("pretty.js\n(1 match)"),
    "The search result was found in the minified (pretty.js) source"
  );

  await closeProjectSearch(dbg);

  await selectSource(dbg, "pretty.js");
  await waitForSelectedSource(dbg, "pretty.js");

  info("Pretty print the source");
  await prettyPrint(dbg);

  await openProjectSearch(dbg);
  fileResults = await doProjectSearch(dbg, "stuff");

  is(fileResults.length, 1, "Only one result was found");
  ok(
    fileResults[0].innerText.includes("pretty.js:formatted\n(1 match)"),
    "The search result was found in the prettyified (pretty.js:formatted) source"
  );
});

// Test expanding search results to reveal the search matches.
add_task(async function testExpandSearchResultsToShowMatches() {
  const dbg = await initDebugger("doc-react.html", "App.js");

  await openProjectSearch(dbg);
  await doProjectSearch(dbg, "we");

  is(getExpandedResultsCount(dbg), 18);

  const collapsedNodes = findAllElements(dbg, "projectSearchCollapsed");
  is(collapsedNodes.length, 1);

  collapsedNodes[0].click();

  is(getExpandedResultsCount(dbg), 226);
});

// Test the prioritization of source-mapped files. (Bug 1642778)
add_task(async function testOriginalFilesAsPrioritizedOverGeneratedFiles() {
  const dbg = await initDebugger("doc-react.html", "App.js");

  await openProjectSearch(dbg);
  const fileResults = await doProjectSearch(dbg, "componentDidMount");

  is(getExpandedResultsCount(dbg), 8);

  ok(
    fileResults[0].innerText.includes(
      "browser/devtools/client/debugger/test/mochitest/examples/react/build/App.js"
    ),
    "The first item should be the original (prettified) file"
  );

  ok(
    findAllElements(dbg, "projectSearchExpandedResults")[0].innerText.endsWith(
      "componentDidMount() {"
    ),
    "The first result match in the original file is correct"
  );
});

function openProjectSearch(dbg) {
  info("Opening the project search panel");
  synthesizeKeyShortcut("CmdOrCtrl+Shift+F");
  return waitForState(
    dbg,
    state => dbg.selectors.getActiveSearch() === "project"
  );
}

async function doProjectSearch(dbg, searchTerm) {
  type(dbg, searchTerm);
  pressKey(dbg, "Enter");
  return waitForSearchResults(dbg);
}

async function waitForSearchResults(dbg) {
  await waitForState(dbg, state => state.projectTextSearch.status === "DONE");
  return findAllElements(dbg, "projectSearchFileResults");
}

function closeProjectSearch(dbg) {
  info("Closing the project search panel");
  synthesizeKeyShortcut("CmdOrCtrl+Shift+F");
  return waitForState(dbg, state => !dbg.selectors.getActiveSearch());
}

async function selectResultMatch(dbg) {
  const select = waitForState(dbg, () => !dbg.selectors.getActiveSearch());
  await clickElement(dbg, "fileMatch");
  return select;
}

function getExpandedResultsCount(dbg) {
  return findAllElements(dbg, "projectSearchExpandedResults").length;
}
