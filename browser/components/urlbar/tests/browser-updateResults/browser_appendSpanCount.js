/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that when the view updates itself and appends new rows,
// the new rows start out hidden when they exceed the current visible result
// span count.  It includes a tip result so that it tests a row with > 1 result
// span.

"use strict";

add_task(async function viewUpdateAppendHidden() {
  // We'll use this test provider to test specific results.  We assume that
  // history and bookmarks have been cleared (by init() above).
  let provider = new DelayingTestProvider();
  UrlbarProvidersManager.registerProvider(provider);
  registerCleanupFunction(() => {
    UrlbarProvidersManager.unregisterProvider(provider);
  });

  // We do two searches below without closing the panel.  Use "firefox cach" as
  // the first query and "firefox cache" as the second so that (1) an
  // intervention tip is triggered both times but also so that (2) the queries
  // are different each time.
  let baseQuery = "firefox cache";
  let queries = [baseQuery.substring(0, baseQuery.length - 1), baseQuery];
  let maxResults = UrlbarPrefs.get("maxRichResults");

  let queryStrings = [];
  for (let i = 0; i < maxResults; i++) {
    queryStrings.push(`${baseQuery} ${i}`);
  }

  // First search: Trigger the intervention tip and a view full of search
  // suggestions.
  provider._results = queryStrings.map(
    suggestion =>
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        {
          query: queries[0],
          suggestion,
          lowerCaseSuggestion: suggestion.toLocaleLowerCase(),
          engine: Services.search.defaultEngine.name,
        }
      )
  );
  provider.finishQueryPromise = Promise.resolve();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: queries[0],
  });

  // Sanity check the tip result and row count.
  let tipResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    tipResult.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "Result at index 1 is a tip"
  );
  let tipResultSpan = UrlbarUtils.getSpanForResult(
    tipResult.element.row.result
  );
  Assert.greater(tipResultSpan, 1, "Sanity check: Tip has large result span");
  let expectedRowCount = maxResults - tipResultSpan + 1;
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    expectedRowCount,
    "Sanity check: Initial row count takes tip result span into account"
  );

  // Second search: Change the provider's results so that it has enough history
  // to fill up the view.  Search suggestion rows cannot be updated to history
  // results, so the view will append the history results as new rows.
  provider._results = queryStrings.map(title => {
    let url = "http://example.com/" + title;
    return new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      {
        title,
        url,
        displayUrl: "http://example.com/" + title,
      }
    );
  });

  // Don't allow the search to finish until we check the updated rows.  We'll
  // accomplish that by adding a mutation observer on the rows and delaying
  // resolving the provider's finishQueryPromise.  When all new rows have been
  // added, we expect the new row count to be:
  //
  //     expectedRowCount       // the original row count
  //   + (expectedRowCount - 2) // the newly added history row count (hidden)
  //   --------------------------
  //   (2 * expectedRowCount) - 2
  //
  // The `- 2` subtracts the heuristic and tip result.
  let newExpectedRowCount = 2 * expectedRowCount - 2;
  let mutationPromise = new Promise(resolve => {
    let observer = new MutationObserver(mutations => {
      let childCount = UrlbarTestUtils.getResultCount(window);
      info(`Rows mutation observer called, childCount now ${childCount}`);
      if (newExpectedRowCount <= childCount) {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(UrlbarTestUtils.getResultsContainer(window), {
      childList: true,
    });
  });

  // Now do the second search but don't wait for it to finish.
  let resolveQuery;
  provider.finishQueryPromise = new Promise(
    resolve => (resolveQuery = resolve)
  );
  let queryPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: queries[1],
  });

  // Wait for the history rows to be added.
  await mutationPromise;

  // Check the rows.  We can't use UrlbarTestUtils.getDetailsOfResultAt() here
  // because it waits for the query to finish.
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    newExpectedRowCount,
    "New expected row count"
  );
  // stale search rows
  let rows = UrlbarTestUtils.getResultsContainer(window).children;
  for (let i = 2; i < expectedRowCount; i++) {
    let row = rows[i];
    Assert.equal(
      row.result.type,
      UrlbarUtils.RESULT_TYPE.SEARCH,
      `Result at index ${i} is a search result`
    );
    Assert.ok(
      BrowserTestUtils.isVisible(row),
      `Search result at index ${i} is visible`
    );
    Assert.equal(
      row.getAttribute("stale"),
      "true",
      `Search result at index ${i} is stale`
    );
  }
  // new hidden history rows
  for (let i = expectedRowCount; i < newExpectedRowCount; i++) {
    let row = rows[i];
    Assert.equal(
      row.result.type,
      UrlbarUtils.RESULT_TYPE.URL,
      `Result at index ${i} is a URL result`
    );
    Assert.ok(
      !BrowserTestUtils.isVisible(row),
      `URL result at index ${i} is hidden`
    );
    Assert.ok(
      !row.hasAttribute("stale"),
      `URL result at index ${i} is not stale`
    );
  }

  // Finish the query, and we're done.
  resolveQuery();
  await queryPromise;

  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.handleRevert();

  // We unregister the provider above in a cleanup function so we don't
  // accidentally interfere with later tests, but do it here too in case we add
  // more tasks to this test.  It's harmless to call more than once.
  UrlbarProvidersManager.unregisterProvider(provider);
});
