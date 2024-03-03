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

  // Increase the timeout of the stale-rows timer so it doesn't interfere with
  // this test, which specifically tests what happens before the timer fires.
  let originalRemoveStaleRowsTimeout = UrlbarView.removeStaleRowsTimeout;
  UrlbarView.removeStaleRowsTimeout = 30000;

  registerCleanupFunction(() => {
    UrlbarView.removeStaleRowsTimeout = originalRemoveStaleRowsTimeout;
    UrlbarProvidersManager.unregisterProvider(gProvider);
  });
});

// Does one query that fills up the view with search suggestions, starts a
// second query that returns a history result, and cancels it before it can
// finish but after the view is updated. Regardless of `showExposureResults`,
// the history result should not trigger an exposure since it never had a chance
// to be visible in the view.
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
  // finish.
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
  let rows = UrlbarTestUtils.getResultsContainer(window);
  let expectedCount = MAX_RESULT_COUNT + 1;
  if (showExposureResults) {
    expectedCount++;
  }
  Assert.equal(
    rows.children.length,
    expectedCount,
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
  let expected = [
    { source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS, url: bookmarkUrl },
  ];
  if (showExposureResults) {
    expected.unshift({
      source: UrlbarUtils.RESULT_SOURCE.HISTORY,
      url: historyUrl,
    });
  }
  for (let i = 0; i < expected.length; i++) {
    let { source, url } = expected[i];
    let row = rows.children[MAX_RESULT_COUNT + i];
    Assert.ok(row, `rows[${i}] should exist`);
    Assert.ok(BrowserTestUtils.isHidden(row), `rows[${i}] should be hidden`);
    Assert.equal(
      row.result.type,
      UrlbarUtils.RESULT_TYPE.URL,
      `rows[${i}].result.type should be URL`
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

  // No exposure should have been recorded since the history result was never
  // visible.
  assertExposureTelemetry([]);

  // Clean up.
  queryResolver.resolve();
  await queryPromise;
  await SpecialPowers.popPrefEnv();
  Services.fog.testResetFOG();
}

// Does one query that underfills the view and then a second query that returns
// a search suggestion. The search suggestion should be appended and trigger an
// exposure. When `showExposureResults` is true, it should also be shown. After
// the view is updated, it shouldn't matter whether or not the second query is
// canceled.
add_task(async function exposure_append() {
  for (let showExposureResults of [true, false]) {
    for (let cancelSecondQuery of [true, false]) {
      await do_exposure_append({
        showExposureResults,
        cancelSecondQuery,
      });
    }
  }
});

async function do_exposure_append({ showExposureResults, cancelSecondQuery }) {
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
  // results but let us test the specific case where the query doesn't finish.
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
}

// Does one query that returns a search suggestion and then a second query that
// returns a new search suggestion. The new search suggestion can replace the
// old one, so it should trigger an exposure. When `showExposureResults` is
// true, it should actually replace it. After the view is updated, it shouldn't
// matter whether or not the second query is canceled.
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
  // results but let us test the specific case where the query doesn't finish.
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
