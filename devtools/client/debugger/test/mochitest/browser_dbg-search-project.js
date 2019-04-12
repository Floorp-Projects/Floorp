/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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
  return findAllElements(dbg, "projectSerchExpandedResults").length;
}

function getResultsFiles(dbg) {
  const matches = dbg.selectors
    .getTextSearchResults()
    .map(file => file.matches);

  return [...matches].length;
}

// Testing project search
add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html", "switching-01");

  await selectSource(dbg, "switching-01");

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
  ok(selectedSource.url.includes("switching-01"));
  await waitForLoadedSource(dbg, "switching-01");
});

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
