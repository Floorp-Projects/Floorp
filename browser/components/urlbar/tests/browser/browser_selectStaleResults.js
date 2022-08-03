/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that arrowing down and up through the view's results
// works correctly with regard to stale results.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
});

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quickactions", false]],
  });

  // Increase the timeout of the remove-stale-rows timer so that it doesn't
  // interfere with the tests.
  let originalRemoveStaleRowsTimeout = UrlbarView.removeStaleRowsTimeout;
  UrlbarView.removeStaleRowsTimeout = 1000;
  registerCleanupFunction(() => {
    UrlbarView.removeStaleRowsTimeout = originalRemoveStaleRowsTimeout;
  });
});

// This tests the case where queryContext.results.length < the number of rows in
// the view, i.e., the view contains stale rows.
add_task(async function viewContainsStaleRows() {
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
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "x",
    fireInputEvent: true,
  });

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
  info("Waiting for 'stale' attribute to be removed... ");
  await mutationPromise;

  // Now arrow down.  The search, which is still ongoing, will now stop and the
  // view won't be updated anymore.
  EventUtils.synthesizeKey("KEY_ArrowDown");

  // Wait for the search to stop.
  info("Waiting for the search to stop... ");
  await gURLBar.lastQueryContextPromise;

  // The query context for the last search ("xx") should contain only
  // halfResults + 1 results (+ 1 for the heuristic).
  Assert.ok(gURLBar.controller._lastQueryContextWrapper);
  let { queryContext } = gURLBar.controller._lastQueryContextWrapper;
  Assert.ok(queryContext);
  Assert.equal(queryContext.results.length, halfResults + 1);

  // But there should be maxResults visible rows in the view.
  let items = Array.from(gURLBar.view._rows.children).filter(r =>
    gURLBar.view._isElementVisible(r)
  );
  Assert.equal(items.length, maxResults);

  // Arrow down through all the results.  After arrowing down from the last "xx"
  // result, the stale "x" results should be selected.  We should *not* enter
  // the one-off search buttons at that point.
  for (let i = 1; i < maxResults; i++) {
    Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), i);
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.equal(result.element.row.result.rowIndex, i);
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }

  // Now the first one-off should be selected.
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), -1);
  Assert.equal(gURLBar.view.oneOffSearchButtons.selectedButtonIndex, 0);

  // Arrow back up through all the results.
  for (let i = maxResults - 1; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), i);
  }

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// This tests the case where, before the search finishes, stale results have
// been removed and replaced with non-stale results.
add_task(async function staleReplacedWithFresh() {
  // For this test, we need one set of results that's added quickly and another
  // set that's added after a delay.  We do an initial search and wait for both
  // sets to be added.  Then we do another search, but this time only wait for
  // the fast results to be added, and then we arrow down to stop the search
  // before the delayed results are added.  The order in which things should
  // happen after the second search goes like this:
  //
  // (1) second search
  // (2) fast results are added
  // (3) remove-stale-rows timer fires and removes stale rows (the rows from
  //     the delayed set of results from the first search)
  // (4) we arrow down to stop the search
  //
  // We use history for the fast results and a slow search engine for the
  // delayed results.
  //
  // NB: If this test ends up failing, it may be because the remove-stale-rows
  // timer fires before the history results are added.  i.e., steps 2 and 3
  // above happen out of order.  If that happens, try increasing
  // UrlbarView.removeStaleRowsTimeout above.

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  // Enable search suggestions, and add an engine that returns suggestions on a
  // delay.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngineSlow.xml"
  );
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.moveEngine(engine, 0);
  await Services.search.setDefault(engine);

  let maxResults = UrlbarPrefs.get("maxRichResults");

  // Add enough visits to pages with "test" in the title to fill up the entire
  // view.
  for (let i = 0; i < maxResults; i++) {
    await PlacesTestUtils.addVisits({
      uri: "http://example.com/" + i,
      title: "test" + i,
    });
  }

  gURLBar.focus();

  // Search for "tes" and wait for the search to finish.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "tes",
    fireInputEvent: true,
  });

  // Sanity check the results.  They should be:
  //
  //   tes -- Search with searchSuggestionEngineSlow [heuristic]
  //   tesfoo [search suggestion]
  //   tesbar [search suggestion]
  //   test9 [history]
  //   test8 [history]
  //   test7 [history]
  //   test6 [history]
  //   test5 [history]
  //   test4 [history]
  //   test3 [history]
  let count = UrlbarTestUtils.getResultCount(window);
  Assert.equal(count, maxResults);
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(result.heuristic);
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.ok(result.searchParams);
  Assert.equal(result.searchParams.suggestion, "tesfoo");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.ok(result.searchParams);
  Assert.equal(result.searchParams.suggestion, "tesbar");
  for (let i = 3; i < maxResults; i++) {
    result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
    Assert.equal(result.title, "test" + (maxResults - i + 2));
  }

  // Below we'll do a search for "test" *but* not wait for the two search
  // suggestion results to be added.  We'll only wait for the history results to
  // be added.  To determine when the history results are added, use a mutation
  // listener on the node containing the rows, and wait until the title of the
  // next-to-last row is "test2".  At that point, the results should be:
  //
  //   test -- Search with searchSuggestionEngineSlow
  //   test9
  //   test8
  //   test7
  //   test6
  //   test5
  //   test4
  //   test3
  //   test2
  //   test1
  let mutationPromise = new Promise(resolve => {
    let observer = new MutationObserver(mutations => {
      let row = gURLBar.view._rows.children[maxResults - 2];
      if (row && row._elements.get("title").textContent == "test2") {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(gURLBar.view._rows, {
      subtree: true,
      characterData: true,
      childList: true,
      attributes: true,
    });
  });

  // Now type a "t" so that we search for "test", but only wait for history
  // results to be added, as described above.
  EventUtils.synthesizeKey("t");
  info("Waiting for the 'test2' row... ");
  await mutationPromise;

  // Now arrow down.  The search, which is still ongoing, will now stop and the
  // view won't be updated anymore.
  EventUtils.synthesizeKey("KEY_ArrowDown");

  // Wait for the search to stop.
  info("Waiting for the search to stop... ");
  await gURLBar.lastQueryContextPromise;

  // Sanity check the results.  They should be as described above.
  count = UrlbarTestUtils.getResultCount(window);
  Assert.equal(count, maxResults);
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(result.heuristic);
  Assert.equal(result.element.row.result.rowIndex, 0);
  for (let i = 1; i < maxResults; i++) {
    result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
    Assert.equal(result.title, "test" + (maxResults - i));
    Assert.equal(result.element.row.result.rowIndex, i);
  }

  // Arrow down through all the results.  After arrowing down from "test3", we
  // should continue on to "test2".  We should *not* enter the one-off search
  // buttons at that point.
  for (let i = 1; i < maxResults; i++) {
    Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), i);
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }

  // Now the first one-off should be selected.
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), -1);
  Assert.equal(gURLBar.view.oneOffSearchButtons.selectedButtonIndex, 0);

  // Arrow back up through all the results.
  for (let i = maxResults - 1; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), i);
  }

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
  await SpecialPowers.popPrefEnv();
  await Services.search.setDefault(oldDefaultEngine);
});
