/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the search bar retains previous query on re-opening.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const {
    selectors: { getActiveSearch },
  } = dbg;
  await selectSource(dbg, "simple1.js");

  // Open search bar
  pressKey(dbg, "fileSearch");
  await waitFor(() => getActiveSearch() === "file");
  is(getActiveSearch(), "file");

  // Type a search query
  type(dbg, "con");
  await waitForSearchState(dbg);
  is(findElement(dbg, "fileSearchInput").value, "con");
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
  is(findElement(dbg, "fileSearchInput").value, "con");
});
