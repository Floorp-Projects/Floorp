/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the search bar retains previous query on re-opening.

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const {
    selectors: { getActiveSearch, getFileSearchQuery },
  } = dbg;
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source.url);

  // Open search bar
  pressKey(dbg, "fileSearch");
  await waitFor(() => getActiveSearch() === "file");
  is(getActiveSearch(), "file");

  // Type a search query
  type(dbg, "con");
  await waitForSearchState(dbg);
  is(getFileSearchQuery(), "con");
  is(getCM(dbg).state.search.query, "con");

  // Close the search bar
  pressKey(dbg, "Escape");
  await waitFor(() => getActiveSearch() === null);
  is(getActiveSearch(), null);

  // Re-open search bar
  pressKey(dbg, "fileSearch");
  await waitFor(() => getActiveSearch() === "file");
  is(getActiveSearch(), "file");

  // Test for the retained query
  is(getCM(dbg).state.search.query, "con");
  await waitForDispatch(dbg, "UPDATE_FILE_SEARCH_QUERY");
  is(getFileSearchQuery(), "con");
});

function waitForSearchState(dbg) {
  return waitForState(dbg, () => getCM(dbg).state.search);
}
