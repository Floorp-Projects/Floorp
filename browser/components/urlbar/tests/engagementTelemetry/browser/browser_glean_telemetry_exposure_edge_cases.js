/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests edge cases related to the exposure event and view updates.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
});

const MAX_RESULT_COUNT = 10;

let gProvider;

add_setup(async function () {
  await initExposureTest();

  await SpecialPowers.pushPrefEnv({
    set: [
      // Make absolutely sure the panel stays open during the test. There are
      // spurious blurs on WebRender TV tests as the test starts that cause the
      // panel to close and the query to be canceled, resulting in intermittent
      // failures without this.
      ["ui.popup.disable_autohide", true],

      // Set maxRichResults for sanity.
      ["browser.urlbar.maxRichResults", MAX_RESULT_COUNT],
    ],
  });

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  gProvider = new TestProvider();
  UrlbarProvidersManager.registerProvider(gProvider);

  // This test specifically checks the view's behavior before and after it
  // removes stale rows, so it needs to control when that occurs. There are two
  // times the view removes stale rows: (1) when the stale-rows timer fires, (2)
  // when a query finishes. We prevent (1) from occuring by increasing the
  // timer's timeout so it never fires during the test. We'll rely on (2) to
  // trigger stale rows removal.
  let originalRemoveStaleRowsTimeout = UrlbarView.removeStaleRowsTimeout;
  UrlbarView.removeStaleRowsTimeout = 30000;

  registerCleanupFunction(() => {
    UrlbarView.removeStaleRowsTimeout = originalRemoveStaleRowsTimeout;
    UrlbarProvidersManager.unregisterProvider(gProvider);
  });
});

// Does the following:
//
// 1. Starts and finishes a query that fills up the view
// 2. Starts a second query with results that cannot replace rows from the first
//    query and that therefore must be appended and hidden
// 3. Cancels the second query before it finishes (so that stale rows are not
//    removed)
//
// Results in the second query should not trigger an exposure. They can never be
// visible in the view since the second query is canceled before stale rows are
// removed.
add_task(async function noExposure() {
  for (let showExposureResults of [true, false]) {
    await do_noExposure(showExposureResults);
  }
});

async function do_noExposure(showExposureResults) {
  info("Starting do_noExposure: " + JSON.stringify({ showExposureResults }));

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.exposureResults", "history"],
      ["browser.urlbar.showExposureResults", showExposureResults],
    ],
  });

  // Make the provider return enough search suggestions to fill the view.
  gProvider.results = [];
  for (let i = 0; i < MAX_RESULT_COUNT; i++) {
    gProvider.results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        {
          suggestion: "suggestion " + i,
          engine: Services.search.defaultEngine.name,
        }
      )
    );
  }

  // Do the first query to fill the view with search suggestions.
  info("Doing first query");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 1",
  });

  // Now make the provider return a history result and bookmark. If
  // `showExposureResults` is true, the history result will be added to the view
  // but it should be hidden since the view is already full. If it's false, it
  // shouldn't be added at all. The bookmark will always be added, which will
  // tell us when the view has been updated either way. (It also will be hidden
  // since the view is already full.)
  let historyUrl = "https://example.com/history";
  let bookmarkUrl = "https://example.com/bookmark";
  gProvider.results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: historyUrl }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      { url: bookmarkUrl }
    ),
  ];

  // When the provider's `startQuery()` is called, let it add its results but
  // don't let it return. That will cause the view to be updated with the new
  // results but prevent it from showing hidden rows since the query won't
  // finish (so stale rows won't be removed).
  let queryResolver = Promise.withResolvers();
  gProvider.finishQueryPromise = queryResolver.promise;

  // Observe when the view appends the bookmark row. This will tell us when the
  // view has been updated with the provider's new results. The bookmark row
  // will be hidden since the view is already full with search suggestions.
  let lastRowPromise = promiseLastRowAppended(
    row => row.result.payload.url == bookmarkUrl
  );

  // Now start the second query but don't await it.
  info("Starting second query");
  let queryPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 2",
    reopenOnBlur: false,
  });

  // Wait for the view to be updated.
  info("Waiting for last row");
  let lastRow = await lastRowPromise;
  info("Done waiting for last row");

  Assert.ok(
    BrowserTestUtils.isHidden(lastRow),
    "The new bookmark row should be hidden since the view is full"
  );

  // Make sure the view is full of visible rows as expected, plus the one or two
  // hidden rows for the history and/or bookmark results.
  let expected = [
    {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      type: UrlbarUtils.RESULT_TYPE.URL,
      url: bookmarkUrl,
    },
  ];
  if (showExposureResults) {
    expected.unshift({
      source: UrlbarUtils.RESULT_SOURCE.HISTORY,
      type: UrlbarUtils.RESULT_TYPE.URL,
      url: historyUrl,
    });
  }
  let rows = UrlbarTestUtils.getResultsContainer(window);
  Assert.equal(
    rows.children.length,
    MAX_RESULT_COUNT + expected.length,
    "The view has the expected number of rows"
  );

  // Check the visible rows.
  for (let i = 0; i < MAX_RESULT_COUNT; i++) {
    let row = rows.children[i];
    Assert.ok(BrowserTestUtils.isVisible(row), `rows[${i}] should be visible`);
    Assert.ok(
      row.result.type == UrlbarUtils.RESULT_TYPE.SEARCH,
      `rows[${i}].result.type should be SEARCH`
    );
    // The heuristic won't have a suggestion so skip it.
    if (i > 0) {
      Assert.ok(
        row.result.payload.suggestion,
        `rows[${i}] should have a suggestion`
      );
    }
  }

  // Check the hidden history and/or bookmark rows.
  for (let i = 0; i < expected.length; i++) {
    let { source, type, url } = expected[i];
    let row = rows.children[MAX_RESULT_COUNT + i];
    Assert.ok(row, `rows[${i}] should exist`);
    Assert.ok(BrowserTestUtils.isHidden(row), `rows[${i}] should be hidden`);
    Assert.equal(
      row.result.type,
      type,
      `rows[${i}].result.type should be as expected`
    );
    Assert.equal(
      row.result.source,
      source,
      `rows[${i}].result.source should be as expected`
    );
    Assert.equal(
      row.result.payload.url,
      url,
      `rows[${i}] URL should be as expected`
    );
  }

  // Close the view. Blur the urlbar to end the session.
  info("Closing view and blurring");
  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.blur();

  // No exposure should have been recorded since the history result could never
  // be visible.
  assertExposureTelemetry([]);

  // Clean up.
  queryResolver.resolve();
  await queryPromise;
  await SpecialPowers.popPrefEnv();
  Services.fog.testResetFOG();
  gProvider.finishQueryPromise = null;
}

// Does the following:
//
// 1. Starts and finishes a query that underfills the view
// 2. Starts a second query
// 3. Waits for rows from the second query to be appended. They will be
//    immediately visible since the first query underfilled the view.
// 4. Either cancels the second query (so stale rows are not removed) or waits
//    for it to finish (so stale rows are removed)
//
// Results in the second query should trigger an exposure since they are made
// visible in step 3. Step 4 should not actually matter.
add_task(async function exposure_append_underfilled() {
  for (let showExposureResults of [true, false]) {
    for (let cancelSecondQuery of [true, false]) {
      await do_exposure_append_underfilled({
        showExposureResults,
        cancelSecondQuery,
      });
    }
  }
});

async function do_exposure_append_underfilled({
  showExposureResults,
  cancelSecondQuery,
}) {
  info(
    "Starting do_exposure_append: " +
      JSON.stringify({ showExposureResults, cancelSecondQuery })
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.exposureResults", "search_suggest"],
      ["browser.urlbar.showExposureResults", showExposureResults],
    ],
  });

  // Make the provider return no results at first.
  gProvider.results = [];

  // Do the first query to open the view.
  info("Doing first query");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 1",
  });

  // Now make the provider return a search suggestion and a bookmark. If
  // `showExposureResults` is true, the suggestion should be added to the view
  // and be visible immediately. If it's false, it shouldn't be added at
  // all. The bookmark will always be added, which will tell us when the view
  // has been updated either way.
  let newSuggestion = "new suggestion";
  let bookmarkUrl = "https://example.com/bookmark";
  gProvider.results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      {
        suggestion: newSuggestion,
        engine: Services.search.defaultEngine.name,
      }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      { url: bookmarkUrl }
    ),
  ];

  // When the provider's `startQuery()` is called, let it add its results but
  // don't let it return. That will cause the view to be updated with the new
  // results but let us test the specific case where the query doesn't finish
  // (so stale rows are not removed).
  let queryResolver = Promise.withResolvers();
  gProvider.finishQueryPromise = queryResolver.promise;

  // Observe when the view appends the bookmark row. This will tell us when the
  // view has been updated with the provider's new results.
  let lastRowPromise = promiseLastRowAppended(
    row => row.result.payload.url == bookmarkUrl
  );

  // Now start the second query but don't await it.
  info("Starting second query");
  let queryPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 2",
    reopenOnBlur: false,
  });

  // Wait for the view to be updated.
  info("Waiting for last row");
  let lastRow = await lastRowPromise;
  info("Done waiting for last row");

  Assert.ok(
    BrowserTestUtils.isVisible(lastRow),
    "The new bookmark row should be visible since the view is not full"
  );

  // Check the new suggestion row.
  let rows = UrlbarTestUtils.getResultsContainer(window);
  let newSuggestionRow = [...rows.children].find(
    r => r.result.payload.suggestion == newSuggestion
  );
  if (showExposureResults) {
    Assert.ok(
      newSuggestionRow,
      "The new suggestion row should have been added"
    );
    Assert.ok(
      BrowserTestUtils.isVisible(newSuggestionRow),
      "The new suggestion row should be visible"
    );
  } else {
    Assert.ok(
      !newSuggestionRow,
      "The new suggestion row should not have been added"
    );
  }

  if (!cancelSecondQuery) {
    // Finish the query.
    queryResolver.resolve();
    await queryPromise;
  }

  // Close the view. Blur the urlbar to end the session.
  info("Closing view and blurring");
  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.blur();

  // If `showExposureResults` is true, the new search suggestion should have
  // been shown; if it's false, it would have been shown. Either way, it should
  // have triggered an exposure.
  assertExposureTelemetry([{ results: "search_suggest" }]);

  // Clean up.
  queryResolver.resolve();
  await queryPromise;
  await SpecialPowers.popPrefEnv();
  Services.fog.testResetFOG();
  gProvider.finishQueryPromise = null;
}

// Does the following:
//
// 1. Starts and finishes a query
// 2. Starts a second query with a result that can replace an existing row from
//    the previous query
// 3. Either cancels the second query (so stale rows are not removed) or waits
//    for it to finish (so stale rows are removed)
//
// The result in the second query should trigger an exposure since it's made
// visible in step 2. Step 3 should not actually matter.
add_task(async function exposure_replace() {
  for (let showExposureResults of [true, false]) {
    for (let cancelSecondQuery of [true, false]) {
      await do_exposure_replace({ showExposureResults, cancelSecondQuery });
    }
  }
});

async function do_exposure_replace({ showExposureResults, cancelSecondQuery }) {
  info(
    "Starting do_exposure_replace: " +
      JSON.stringify({ showExposureResults, cancelSecondQuery })
  );

  // Make the provider return a search suggestion.
  gProvider.results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      {
        suggestion: "suggestion",
        engine: Services.search.defaultEngine.name,
      }
    ),
  ];

  // Do the first query to show the suggestion.
  info("Doing first query");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 1",
  });

  // Set exposure results to search suggestions and hide them. We can't do this
  // before now because that would hide the search suggestions in the first
  // query, and here we're specifically testing the case where a new row
  // replaces an old row, which is only allowed for rows of the same type.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.exposureResults", "search_suggest"],
      ["browser.urlbar.showExposureResults", showExposureResults],
    ],
  });

  // Now make the provider return another search suggestion and a bookmark. If
  // `showExposureResults` is true, the new suggestion should replace the old
  // one in the view and be visible immediately. If it's false, it shouldn't be
  // added at all. The bookmark will always be added, which will tell us when
  // the view has been updated either way.
  let newSuggestion = "new suggestion";
  let bookmarkUrl = "https://example.com/bookmark";
  gProvider.results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      {
        suggestion: newSuggestion,
        engine: Services.search.defaultEngine.name,
      }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      { url: bookmarkUrl }
    ),
  ];

  // When the provider's `startQuery()` is called, let it add its results but
  // don't let it return. That will cause the view to be updated with the new
  // results but let us test the specific case where the query doesn't finish
  // (so stale rows are not removed).
  let queryResolver = Promise.withResolvers();
  gProvider.finishQueryPromise = queryResolver.promise;

  // Observe when the view appends the bookmark row. This will tell us when the
  // view has been updated with the provider's new results.
  let lastRowPromise = promiseLastRowAppended(
    row => row.result.payload.url == bookmarkUrl
  );

  // Now start the second query but don't await it.
  info("Starting second query");
  let queryPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 2",
    reopenOnBlur: false,
  });

  // Wait for the view to be updated.
  info("Waiting for last row");
  let lastRow = await lastRowPromise;
  info("Done waiting for last row");

  Assert.ok(
    BrowserTestUtils.isVisible(lastRow),
    "The new bookmark row should be visible since the view is not full"
  );

  // Check the new suggestion row.
  let rows = UrlbarTestUtils.getResultsContainer(window);
  let newSuggestionRow = [...rows.children].find(
    r => r.result.payload.suggestion == newSuggestion
  );
  if (showExposureResults) {
    Assert.ok(
      newSuggestionRow,
      "The new suggestion row should have replaced the old one"
    );
    Assert.ok(
      BrowserTestUtils.isVisible(newSuggestionRow),
      "The new suggestion row should be visible"
    );
  } else {
    Assert.ok(
      !newSuggestionRow,
      "The new suggestion row should not have been added"
    );
  }

  if (!cancelSecondQuery) {
    // Finish the query.
    queryResolver.resolve();
    await queryPromise;
  }

  // Close the view. Blur the urlbar to end the session.
  info("Closing view and blurring");
  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.blur();

  // If `showExposureResults` is true, the new search suggestion should have
  // been shown; if it's false, it would have been shown. Either way, it should
  // have triggered an exposure.
  assertExposureTelemetry([{ results: "search_suggest" }]);

  // Clean up.
  queryResolver.resolve();
  await queryPromise;
  await SpecialPowers.popPrefEnv();
  Services.fog.testResetFOG();
  gProvider.finishQueryPromise = null;
}

// Does the following:
//
// 1. Starts and finishes a query that fills up the view
// 2. Starts a second query with a result that cannot replace any rows from the
//    first query and that therefore must be appended and hidden
// 3. Finishes the second query
//
// The result in the second query should trigger an exposure since it's made
// visible in step 3.
add_task(async function exposure_append_full() {
  for (let showExposureResults of [true, false]) {
    await do_exposure_append_full(showExposureResults);
  }
});

async function do_exposure_append_full(showExposureResults) {
  info(
    "Starting do_exposure_append_full: " +
      JSON.stringify({ showExposureResults })
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.exposureResults", "history"],
      ["browser.urlbar.showExposureResults", showExposureResults],
    ],
  });

  // Make the provider return enough search suggestions to fill the view.
  gProvider.results = [];
  for (let i = 0; i < MAX_RESULT_COUNT; i++) {
    gProvider.results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        {
          suggestion: "suggestion " + i,
          engine: Services.search.defaultEngine.name,
        }
      )
    );
  }

  // Do the first query to fill the view with search suggestions.
  info("Doing first query");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 1",
  });

  // Now make the provider return a history result and bookmark. If
  // `showExposureResults` is true, the history result will be added to the view
  // but it should be hidden since the view is already full. If it's false, it
  // shouldn't be added at all. The bookmark will always be added, which will
  // tell us when the view has been updated either way. (It also will be hidden
  // since the view is already full.)
  let historyUrl = "https://example.com/history";
  let bookmarkUrl = "https://example.com/bookmark";
  gProvider.results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: historyUrl }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      { url: bookmarkUrl }
    ),
  ];

  // When the provider's `startQuery()` is called, let it add its results but
  // don't let it return. That will cause the view to be updated with the new
  // results but prevent it from showing hidden rows since the query won't
  // finish (so stale rows won't be removed).
  let queryResolver = Promise.withResolvers();
  gProvider.finishQueryPromise = queryResolver.promise;

  // Observe when the view appends the bookmark row. This will tell us when the
  // view has been updated with the provider's new results. The bookmark row
  // will be hidden since the view is already full with search suggestions.
  let lastRowPromise = promiseLastRowAppended(
    row => row.result.payload.url == bookmarkUrl
  );

  // Now start the second query but don't await it.
  info("Starting second query");
  let queryPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 2",
    reopenOnBlur: false,
  });

  // Wait for the view to be updated.
  info("Waiting for last row");
  let lastRow = await lastRowPromise;
  info("Done waiting for last row");

  Assert.ok(
    BrowserTestUtils.isHidden(lastRow),
    "The new bookmark row should be hidden since the view is full"
  );

  // Make sure the view is full of visible rows as expected, plus the one or two
  // hidden rows for the history and bookmark results.
  let expected = [
    {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      type: UrlbarUtils.RESULT_TYPE.URL,
      url: bookmarkUrl,
    },
  ];
  if (showExposureResults) {
    expected.unshift({
      source: UrlbarUtils.RESULT_SOURCE.HISTORY,
      type: UrlbarUtils.RESULT_TYPE.URL,
      url: historyUrl,
    });
  }
  let rows = UrlbarTestUtils.getResultsContainer(window);
  Assert.equal(
    rows.children.length,
    MAX_RESULT_COUNT + expected.length,
    "The view has the expected number of rows"
  );

  // Check the visible rows.
  for (let i = 0; i < MAX_RESULT_COUNT; i++) {
    let row = rows.children[i];
    Assert.ok(BrowserTestUtils.isVisible(row), `rows[${i}] should be visible`);
    Assert.ok(
      row.result.type == UrlbarUtils.RESULT_TYPE.SEARCH,
      `rows[${i}].result.type should be SEARCH`
    );
    // The heuristic won't have a suggestion so skip it.
    if (i > 0) {
      Assert.ok(
        row.result.payload.suggestion,
        `rows[${i}] should have a suggestion`
      );
    }
  }

  // Check the hidden history and bookmark rows.
  for (let i = 0; i < expected.length; i++) {
    let { source, type, url } = expected[i];
    let row = rows.children[MAX_RESULT_COUNT + i];
    Assert.ok(row, `rows[${i}] should exist`);
    Assert.ok(BrowserTestUtils.isHidden(row), `rows[${i}] should be hidden`);
    Assert.equal(
      row.result.type,
      type,
      `rows[${i}].result.type should be as expected`
    );
    Assert.equal(
      row.result.source,
      source,
      `rows[${i}].result.source should be as expected`
    );
    Assert.equal(
      row.result.payload.url,
      url,
      `rows[${i}] URL should be as expected`
    );
  }

  // Now let the query finish (so stale rows are removed).
  queryResolver.resolve();
  info("Waiting for second query to finish");
  await queryPromise;
  info("Second query finished");

  rows = UrlbarTestUtils.getResultsContainer(window);
  Assert.equal(
    rows.children.length,
    // + 1 for the heurustic.
    1 + expected.length,
    "The view has the expected number of rows"
  );

  // Check the visible rows (except the heuristic).
  for (let i = 0; i < expected.length; i++) {
    let { source, type, url } = expected[i];
    let index = i + 1;
    let row = rows.children[index];
    Assert.ok(row, `rows[${index}] should exist`);
    Assert.ok(
      BrowserTestUtils.isVisible(row),
      `rows[${index}] should be visible`
    );
    Assert.equal(
      row.result.type,
      type,
      `rows[${index}].result.type should be as expected`
    );
    Assert.equal(
      row.result.source,
      source,
      `rows[${index}].result.source should be as expected`
    );
    Assert.equal(
      row.result.payload.url,
      url,
      `rows[${index}] URL should be as expected`
    );
  }

  // Close the view. Blur the urlbar to end the session.
  info("Closing view and blurring");
  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.blur();

  // An exposure for the history result should have been recorded.
  assertExposureTelemetry([{ results: "history" }]);

  // Clean up.
  await SpecialPowers.popPrefEnv();
  Services.fog.testResetFOG();
  gProvider.finishQueryPromise = null;
}

// Does the following:
//
// 1. Starts and finishes a query that fills up the view
// 2. Starts a second query with results that cannot replace rows from the first
//    query and that therefore must be appended and hidden
// 3. Before the second query finishes (i.e., before stale rows are removed),
//    starts and finishes a third query (after which stale rows are removed)
//
// Results in the third query should trigger an exposure since they become
// visible when the query finishes (and stale rows are removed) in step 3.
// Results in the second query should not trigger an exposure since they could
// never be visible since the query is canceled before stale rows are removed.
add_task(async function exposure_append_full_twice() {
  for (let showExposureResults of [true, false]) {
    await do_exposure_append_full_twice(showExposureResults);
  }
});

async function do_exposure_append_full_twice(showExposureResults) {
  info(
    "Starting do_exposure_append_full_twice: " +
      JSON.stringify({ showExposureResults })
  );

  // Exposure results are history and tab.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.exposureResults", "history,tab"],
      ["browser.urlbar.showExposureResults", showExposureResults],
    ],
  });

  // Make the provider return enough search suggestions to fill the view.
  gProvider.results = [];
  for (let i = 0; i < MAX_RESULT_COUNT; i++) {
    gProvider.results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        {
          suggestion: "suggestion " + i,
          engine: Services.search.defaultEngine.name,
        }
      )
    );
  }

  // Do the first query to fill the view with search suggestions.
  info("Doing first query");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 1",
  });

  // Now make the provider return a history result, tab, and bookmark. If
  // `showExposureResults` is true, the history and tab results will be added to
  // the view but they should be hidden since the view is already full. If it's
  // false, they shouldn't be added at all. The bookmark will always be added,
  // which will tell us when the view has been updated either way. (It also will
  // be hidden since the view is already full.)
  let historyUrl = "https://example.com/history";
  let tabUrl = "https://example.com/tab";
  let bookmarkUrl = "https://example.com/bookmark";
  gProvider.results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: historyUrl }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_SOURCE.TABS,
      { url: tabUrl }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      { url: bookmarkUrl }
    ),
  ];

  // When the provider's `startQuery()` is called, let it add its results but
  // don't let it return. That will cause the view to be updated with the new
  // results but prevent it from showing hidden rows since the query won't
  // finish (so stale rows won't be removed).
  let secondQueryResolver = Promise.withResolvers();
  gProvider.finishQueryPromise = secondQueryResolver.promise;

  // Observe when the view appends the bookmark row. This will tell us when the
  // view has been updated with the provider's new results. The bookmark row
  // will be hidden since the view is already full with search suggestions.
  let lastRowPromise = promiseLastRowAppended(
    row => row.result.payload.url == bookmarkUrl
  );

  // Now start the second query but don't await it.
  info("Starting second query");
  let secondQueryPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 2",
    reopenOnBlur: false,
  });

  // Wait for the view to be updated.
  info("Waiting for last row");
  let lastRow = await lastRowPromise;
  info("Done waiting for last row");

  Assert.ok(
    BrowserTestUtils.isHidden(lastRow),
    "The new bookmark row should be hidden since the view is full"
  );

  // Make sure the view is full of visible rows as expected, plus the one or
  // three hidden rows for the history, tab, and bookmark results.
  let expected = [
    {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      type: UrlbarUtils.RESULT_TYPE.URL,
      url: bookmarkUrl,
    },
  ];
  if (showExposureResults) {
    expected.unshift(
      {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        type: UrlbarUtils.RESULT_TYPE.URL,
        url: historyUrl,
      },
      {
        source: UrlbarUtils.RESULT_SOURCE.TABS,
        type: UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
        url: tabUrl,
      }
    );
  }
  let rows = UrlbarTestUtils.getResultsContainer(window);
  Assert.equal(
    rows.children.length,
    MAX_RESULT_COUNT + expected.length,
    "The view has the expected number of rows"
  );

  // Check the visible rows.
  for (let i = 0; i < MAX_RESULT_COUNT; i++) {
    let row = rows.children[i];
    Assert.ok(BrowserTestUtils.isVisible(row), `rows[${i}] should be visible`);
    Assert.ok(
      row.result.type == UrlbarUtils.RESULT_TYPE.SEARCH,
      `rows[${i}].result.type should be SEARCH`
    );
    // The heuristic won't have a suggestion so skip it.
    if (i > 0) {
      Assert.ok(
        row.result.payload.suggestion,
        `rows[${i}] should have a suggestion`
      );
    }
  }

  // Check the hidden history, tab, and bookmark rows.
  for (let i = 0; i < expected.length; i++) {
    let { source, type, url } = expected[i];
    let row = rows.children[MAX_RESULT_COUNT + i];
    Assert.ok(row, `rows[${i}] should exist`);
    Assert.ok(BrowserTestUtils.isHidden(row), `rows[${i}] should be hidden`);
    Assert.equal(
      row.result.type,
      type,
      `rows[${i}].result.type should be as expected`
    );
    Assert.equal(
      row.result.source,
      source,
      `rows[${i}].result.source should be as expected`
    );
    Assert.equal(
      row.result.payload.url,
      url,
      `rows[${i}] URL should be as expected`
    );
  }

  // Now make the provider return only a history result.
  gProvider.results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: historyUrl }
    ),
  ];

  // Without waiting for the second query to finish (i.e., before stale rows are
  // removed), do a third query and allow it to finish (so stale rows are
  // removed). An exposure should be recorded for the history result since it's
  // present in the third query. An exposure should not be recorded for the tab
  // result because it could not have been visible since the second query did
  // not finish.

  let thirdQueryStartedPromise = new Promise(resolve => {
    let queryListener = {
      onQueryStarted: () => {
        gURLBar.controller.removeQueryListener(queryListener);
        resolve();
      },
    };
    gURLBar.controller.addQueryListener(queryListener);
  });

  info("Starting third query");
  let thirdQueryPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test 3",
    reopenOnBlur: false,
  });

  // The test provider's `startQuery()` is still awaiting its
  // `finishQueryPromise`, so we need to resolve it so the provider can respond
  // to the third query. But before we do that, we need to make sure the third
  // query has started and canceled the second query because otherwise the
  // second query could finish and cause stale rows to be removed.
  info("Waiting for third query to start");
  await thirdQueryStartedPromise;
  info("Resolving provider's finishQueryPromise");
  secondQueryResolver.resolve();

  // Now wait for the third query to finish.
  info("Waiting for third query to finish");
  await thirdQueryPromise;

  expected = [];
  if (showExposureResults) {
    expected.unshift({
      source: UrlbarUtils.RESULT_SOURCE.HISTORY,
      type: UrlbarUtils.RESULT_TYPE.URL,
      url: historyUrl,
    });
  }

  rows = UrlbarTestUtils.getResultsContainer(window);
  Assert.equal(
    rows.children.length,
    // + 1 for the heurustic.
    1 + expected.length,
    "The view has the expected number of rows"
  );

  // Check the history row.
  for (let i = 0; i < expected.length; i++) {
    let { source, type, url } = expected[i];
    let index = i + 1;
    let row = rows.children[index];
    Assert.ok(row, `rows[${index}] should exist`);
    Assert.ok(
      BrowserTestUtils.isVisible(row),
      `rows[${index}] should be visible`
    );
    Assert.equal(
      row.result.type,
      type,
      `rows[${index}].result.type should be as expected`
    );
    Assert.equal(
      row.result.source,
      source,
      `rows[${index}].result.source should be as expected`
    );
    Assert.equal(
      row.result.payload.url,
      url,
      `rows[${index}] URL should be as expected`
    );
  }

  // Close the view. Blur the urlbar to end the session.
  info("Closing view and blurring");
  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.blur();

  // An exposure only for the history result should have been recorded. If an
  // exposure was also incorrectly recorded for the tab result, this will fail
  // with "history,tab" instead of only "history".
  assertExposureTelemetry([{ results: "history" }]);

  // Clean up.
  await secondQueryPromise;
  await SpecialPowers.popPrefEnv();
  Services.fog.testResetFOG();
  gProvider.finishQueryPromise = null;
}

/**
 * A test provider that doesn't finish startQuery() until `finishQueryPromise`
 * is resolved.
 */
class TestProvider extends UrlbarTestUtils.TestProvider {
  finishQueryPromise = null;

  async startQuery(context, addCallback) {
    for (let result of this.results) {
      addCallback(this, result);
    }
    await this.finishQueryPromise;
  }
}

function promiseLastRowAppended(predicate) {
  return new Promise(resolve => {
    let rows = UrlbarTestUtils.getResultsContainer(window);
    let observer = new MutationObserver(() => {
      let lastRow = rows.children[rows.children.length - 1];
      info(
        "Observed mutation, lastRow.result is: " +
          JSON.stringify(lastRow.result)
      );
      if (predicate(lastRow)) {
        observer.disconnect();
        resolve(lastRow);
      }
    });
    observer.observe(rows, { childList: true });
  });
}
