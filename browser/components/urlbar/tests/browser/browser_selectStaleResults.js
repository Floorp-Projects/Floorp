/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that when the view shows a mix of stale and fresh
// results, arrowing down and up through the results works correctly.

"use strict";

add_task(async function test() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  let maxResults = UrlbarPrefs.get("maxRichResults");
  let halfResults = Math.floor(maxResults / 2);

  // Add enough visits to pages with "xx" in the title to fill up half the view.
  for (let i = 0; i < halfResults; i++) {
    await PlacesTestUtils.addVisits({
      uri: "http://mochi.test:8888/" + i,
      title: "xx" + i,
    });
  }

  // Add enough visits to pages with "x" in the title to fill up the entire
  // view.
  for (let i = 0; i < maxResults; i++) {
    await PlacesTestUtils.addVisits({
      uri: "http://example.com/" + i,
      title: "x" + i,
    });
  }

  gURLBar.focus();

  // Search for "x" and wait for the search to finish.  All the "x" results
  // added above should be in the view.  (Actually one fewer will be in the
  // view due to the heuristic result, but that's not important.)
  await promiseAutocompleteResultPopup("x", window, true);

  // Below we'll do a search for "xx".  Get the row that will show the last
  // result in that search.
  let row = gURLBar.view._rows.children[halfResults];

  // Add a mutation listener on that row.  Wait for its "stale" attribute to be
  // removed.
  let mutationPromise = new Promise(resolve => {
    let observer = new MutationObserver(mutations => {
      for (let mut of mutations) {
        if (mut.attributeName == "stale" && !row.hasAttribute("stale")) {
          observer.disconnect();
          resolve();
          break;
        }
      }
    });
    observer.observe(row, { attributes: true });
  });

  // Type another "x" so that we search for "xx", but don't wait for the search
  // to finish.  Instead, wait for the row's stale attribute to be removed.
  EventUtils.synthesizeKey("x");
  await mutationPromise;

  // Now arrow down.  The search, which is still ongoing, will now stop and the
  // view won't be updated anymore.
  EventUtils.synthesizeKey("KEY_ArrowDown");

  // Wait for the search to stop.
  await gURLBar.lastQueryContextPromise;

  // The query context for the last search ("xx") should contain only
  // halfResults + 1 results (+ 1 for the heuristic).
  Assert.ok(gURLBar.controller._lastQueryContextWrapper);
  let { queryContext } = gURLBar.controller._lastQueryContextWrapper;
  Assert.ok(queryContext);
  Assert.equal(queryContext.results.length, halfResults + 1);

  // But there should be maxResults visible rows in the view.
  let items = Array.from(gURLBar.view._rows.children)
                   .filter(r => gURLBar.view._isRowVisible(r));
  Assert.equal(items.length, maxResults);

  // Arrow down through all the results.  After arrowing down from the last "xx"
  // result, the stale "x" results should be selected.  We should *not* enter
  // the one-off search buttons at that point.
  for (let i = 1; i < maxResults; i++) {
    Assert.equal(UrlbarTestUtils.getSelectedIndex(window), i);
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }

  // Now the first one-off should be selected.
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), -1);
  Assert.equal(gURLBar.view.oneOffSearchButtons.selectedButtonIndex, 0);

  // Arrow back up through all the results.
  for (let i = maxResults - 1; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    Assert.equal(UrlbarTestUtils.getSelectedIndex(window), i);
  }

  await UrlbarTestUtils.promisePopupClose(window,
    () => EventUtils.synthesizeKey("KEY_Escape"));
});
