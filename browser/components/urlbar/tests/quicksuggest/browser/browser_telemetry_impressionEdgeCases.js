/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests abandonment and edge cases related to impressions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.sys.mjs",
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

const { TELEMETRY_SCALARS } = UrlbarProviderQuickSuggest;

const REMOTE_SETTINGS_RESULTS = [
  {
    id: 1,
    url: "https://example.com/sponsored",
    title: "Sponsored suggestion",
    keywords: ["sponsored"],
    click_url: "https://example.com/click",
    impression_url: "https://example.com/impression",
    advertiser: "testadvertiser",
  },
  {
    id: 2,
    url: "https://example.com/nonsponsored",
    title: "Non-sponsored suggestion",
    keywords: ["nonsponsored"],
    click_url: "https://example.com/click",
    impression_url: "https://example.com/impression",
    advertiser: "testadvertiser",
    iab_category: "5 - Education",
  },
];

const SPONSORED_RESULT = REMOTE_SETTINGS_RESULTS[0];

add_setup(async function () {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  // Add a mock engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: REMOTE_SETTINGS_RESULTS,
      },
    ],
  });
});

// Makes sure impression telemetry is not recorded when the urlbar engagement is
// abandoned.
add_task(async function abandonment() {
  Services.telemetry.clearEvents();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "sponsored",
    fireInputEvent: true,
  });
  await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    url: SPONSORED_RESULT.url,
  });
  await UrlbarTestUtils.promisePopupClose(window, () => {
    gURLBar.blur();
  });
  QuickSuggestTestUtils.assertScalars({});
  QuickSuggestTestUtils.assertEvents([]);
});

// Makes sure impression telemetry is not recorded when a quick suggest result
// is not present.
add_task(async function noQuickSuggestResult() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    Services.telemetry.clearEvents();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "noImpression_noQuickSuggestResult",
      fireInputEvent: true,
    });
    await QuickSuggestTestUtils.assertNoQuickSuggestResults(window);
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });
    QuickSuggestTestUtils.assertScalars({});
    QuickSuggestTestUtils.assertEvents([]);
  });
  await PlacesUtils.history.clear();
});

// When a quick suggest result is added to the view but hidden during the view
// update, impression telemetry should not be recorded for it.
add_task(async function hiddenRow() {
  Services.telemetry.clearEvents();

  // Increase the timeout of the remove-stale-rows timer so that it doesn't
  // interfere with this task.
  let originalRemoveStaleRowsTimeout = UrlbarView.removeStaleRowsTimeout;
  UrlbarView.removeStaleRowsTimeout = 30000;
  registerCleanupFunction(() => {
    UrlbarView.removeStaleRowsTimeout = originalRemoveStaleRowsTimeout;
  });

  // Set up a test provider that doesn't add any results until we resolve its
  // `finishQueryPromise`. For the first search below, it will add many search
  // suggestions.
  let maxCount = UrlbarPrefs.get("maxRichResults");
  let results = [];
  for (let i = 0; i < maxCount; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        {
          engine: "Example",
          suggestion: "suggestion " + i,
          lowerCaseSuggestion: "suggestion " + i,
          query: "test",
        }
      )
    );
  }
  let provider = new DelayingTestProvider({ results });
  UrlbarProvidersManager.registerProvider(provider);

  // Open a new tab since we'll load a page below.
  let tab = await BrowserTestUtils.openNewForegroundTab({ gBrowser });

  // Do a normal search and allow the test provider to finish.
  provider.finishQueryPromise = Promise.resolve();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
    fireInputEvent: true,
  });

  // Sanity check the rows. After the heuristic, the remaining rows should be
  // the search results added by the test provider.
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    maxCount,
    "Row count after first search"
  );
  for (let i = 1; i < maxCount; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.equal(
      result.type,
      UrlbarUtils.RESULT_TYPE.SEARCH,
      "Expected result type at index " + i
    );
    Assert.equal(
      result.source,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      "Expected result source at index " + i
    );
  }

  // Now set up a second search that triggers a quick suggest result. Add a
  // mutation listener to the view so we can tell when the quick suggest row is
  // added.
  let mutationPromise = new Promise(resolve => {
    let observer = new MutationObserver(mutations => {
      let rows = UrlbarTestUtils.getResultsContainer(window).children;
      for (let row of rows) {
        if (row.result.providerName == "UrlbarProviderQuickSuggest") {
          observer.disconnect();
          resolve(row);
          return;
        }
      }
    });
    observer.observe(UrlbarTestUtils.getResultsContainer(window), {
      childList: true,
    });
  });

  // Set the test provider's `finishQueryPromise` to a promise that doesn't
  // resolve. That will prevent the search from completing, which will prevent
  // the view from removing stale rows and showing the quick suggest row.
  let resolveQuery;
  provider.finishQueryPromise = new Promise(
    resolve => (resolveQuery = resolve)
  );

  // Start the second search but don't wait for it to finish.
  gURLBar.focus();
  let queryPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: REMOTE_SETTINGS_RESULTS[0].keywords[0],
    fireInputEvent: true,
  });

  // Wait for the quick suggest row to be added to the view. It should be hidden
  // because (a) quick suggest results have a `suggestedIndex`, and rows with
  // suggested indexes can't replace rows without suggested indexes, and (b) the
  // view already contains the maximum number of rows due to the first search.
  // It should remain hidden until the search completes or the remove-stale-rows
  // timer fires. Next, we'll hit enter, which will cancel the search and close
  // the view, so the row should never appear.
  let quickSuggestRow = await mutationPromise;
  Assert.ok(
    BrowserTestUtils.isHidden(quickSuggestRow),
    "Quick suggest row is hidden"
  );

  // Hit enter to pick the heuristic search result. This will cancel the search
  // and notify the quick suggest provider that an engagement occurred.
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Enter");
  });
  await loadPromise;

  // Resolve the test provider's promise finally.
  resolveQuery();
  await queryPromise;

  // The quick suggest provider added a result but it wasn't visible in the
  // view. No impression telemetry should be recorded for it.
  QuickSuggestTestUtils.assertScalars({});
  QuickSuggestTestUtils.assertEvents([]);

  BrowserTestUtils.removeTab(tab);
  UrlbarProvidersManager.unregisterProvider(provider);
  UrlbarView.removeStaleRowsTimeout = originalRemoveStaleRowsTimeout;
});

// When a quick suggest result has not been added to the view, impression
// telemetry should not be recorded for it even if it's the result most recently
// returned by the provider.
add_task(async function notAddedToView() {
  Services.telemetry.clearEvents();

  // Open a new tab since we'll load a page.
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    // Do an initial search that doesn't match any suggestions to make sure
    // there aren't any quick suggest results in the view to start.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "this doesn't match anything",
      fireInputEvent: true,
    });
    await QuickSuggestTestUtils.assertNoQuickSuggestResults(window);
    await UrlbarTestUtils.promisePopupClose(window);

    // Now do a search for a suggestion and hit enter after the provider adds it
    // but before it appears in the view.
    await doEngagementWithoutAddingResultToView(
      REMOTE_SETTINGS_RESULTS[0].keywords[0]
    );

    // The quick suggest provider added a result but it wasn't visible in the
    // view, and no other quick suggest results were visible in the view. No
    // impression telemetry should be recorded.
    QuickSuggestTestUtils.assertScalars({});
    QuickSuggestTestUtils.assertEvents([]);
  });
});

// When a quick suggest result is visible in the view, impression telemetry
// should be recorded for it even if it's not the result most recently returned
// by the provider.
add_task(async function previousResultStillVisible() {
  Services.telemetry.clearEvents();

  // Open a new tab since we'll load a page.
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    // Do a search for the first suggestion.
    let firstSuggestion = REMOTE_SETTINGS_RESULTS[0];
    let index = 1;

    let pingSubmitted = false;
    GleanPings.quickSuggest.testBeforeNextSubmit(() => {
      pingSubmitted = true;
      Assert.equal(
        Glean.quickSuggest.pingType.testGetValue(),
        CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION
      );
      Assert.equal(
        Glean.quickSuggest.improveSuggestExperience.testGetValue(),
        false
      );
      Assert.equal(
        Glean.quickSuggest.blockId.testGetValue(),
        firstSuggestion.id
      );
      Assert.equal(Glean.quickSuggest.isClicked.testGetValue(), false);
      Assert.equal(
        Glean.quickSuggest.matchType.testGetValue(),
        "firefox-suggest"
      );
      Assert.equal(Glean.quickSuggest.position.testGetValue(), index + 1);
    });

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: firstSuggestion.keywords[0],
      fireInputEvent: true,
    });

    await QuickSuggestTestUtils.assertIsQuickSuggest({
      window,
      index,
      url: firstSuggestion.url,
    });

    // Without closing the view, do a second search for the second suggestion
    // and hit enter after the provider adds it but before it appears in the
    // view.
    await doEngagementWithoutAddingResultToView(
      REMOTE_SETTINGS_RESULTS[1].keywords[0],
      index
    );

    // An impression for the first suggestion should be recorded since it's
    // still visible in the view, not the second suggestion.
    QuickSuggestTestUtils.assertScalars({
      [TELEMETRY_SCALARS.IMPRESSION_SPONSORED]: index + 1,
    });
    QuickSuggestTestUtils.assertEvents([
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "engagement",
        object: "impression_only",
        extra: {
          match_type: "firefox-suggest",
          position: String(index + 1),
          suggestion_type: "sponsored",
        },
      },
    ]);
    Assert.ok(pingSubmitted, "Glean ping was submitted");
  });
});

/**
 * Does a search that causes the quick suggest provider to return a result
 * without adding it to the view and then hits enter to load a SERP and create
 * an engagement.
 *
 * @param {string} searchString
 *   The search string.
 * @param {number} previousResultIndex
 *   If the view is already open and showing a quick suggest result, pass its
 *   index here. Otherwise pass -1.
 */
async function doEngagementWithoutAddingResultToView(
  searchString,
  previousResultIndex = -1
) {
  // Set the timeout of the chunk timer to a really high value so that it will
  // not fire. The view updates when the timer fires, which we specifically want
  // to avoid here.
  let originalChunkTimeout = UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS;
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = 30000;
  const cleanup = () => {
    UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = originalChunkTimeout;
  };
  registerCleanupFunction(cleanup);

  // Stub `UrlbarProviderQuickSuggest.getPriority()` to return Infinity.
  let sandbox = sinon.createSandbox();
  let getPriorityStub = sandbox.stub(UrlbarProviderQuickSuggest, "getPriority");
  getPriorityStub.returns(Infinity);

  // Spy on `UrlbarProviderQuickSuggest.onEngagement()`.
  let onEngagementSpy = sandbox.spy(UrlbarProviderQuickSuggest, "onEngagement");

  let sandboxCleanup = () => {
    getPriorityStub?.restore();
    getPriorityStub = null;
    sandbox?.restore();
    sandbox = null;
  };
  registerCleanupFunction(sandboxCleanup);

  // In addition to setting the chunk timeout to a large value above, in order
  // to prevent the view from updating there also needs to be a heuristic
  // provider that takes a long time to add results. Set one up that doesn't add
  // any results until we resolve its `finishQueryPromise`. Set its priority to
  // Infinity too so that only it and the quick suggest provider will be active.
  let provider = new DelayingTestProvider({
    results: [],
    priority: Infinity,
    type: UrlbarUtils.PROVIDER_TYPE.HEURISTIC,
  });
  UrlbarProvidersManager.registerProvider(provider);

  let resolveQuery;
  provider.finishQueryPromise = new Promise(r => (resolveQuery = r));

  // Add a query listener so we can grab the query context.
  let context;
  let queryListener = {
    onQueryStarted: c => (context = c),
  };
  gURLBar.controller.addQueryListener(queryListener);

  // Do a search but don't wait for it to finish.
  gURLBar.focus();
  UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: searchString,
    fireInputEvent: true,
  });

  // Wait for the quick suggest provider to add its result to `context.unsortedResults`.
  let result = await TestUtils.waitForCondition(() => {
    let query = UrlbarProvidersManager.queries.get(context);
    return query?.unsortedResults.find(
      r => r.providerName == "UrlbarProviderQuickSuggest"
    );
  }, "Waiting for quick suggest result to be added to context.unsortedResults");

  gURLBar.controller.removeQueryListener(queryListener);

  // The view should not have updated, so the result's `rowIndex` should still
  // have its initial value of -1.
  Assert.equal(result.rowIndex, -1, "result.rowIndex is still -1");

  // If there's a result from the previous query, assert it's still in the
  // view. Otherwise assume that the view should be closed. These are mostly
  // sanity checks because they should only fail if the telemetry assertions
  // below also fail.
  if (previousResultIndex >= 0) {
    let rows = gURLBar.view.panel.querySelector(".urlbarView-results");
    Assert.equal(
      rows.children[previousResultIndex].result.providerName,
      "UrlbarProviderQuickSuggest",
      "Result already in view is a quick suggest"
    );
  } else {
    Assert.ok(!gURLBar.view.isOpen, "View is closed");
  }

  // Hit enter to load a SERP for the search string. This should notify the
  // quick suggest provider that an engagement occurred.
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Enter");
  });
  await loadPromise;

  let engagementCalls = onEngagementSpy.getCalls().filter(call => {
    let state = call.args[0];
    return state == "engagement";
  });
  Assert.equal(engagementCalls.length, 1, "One engagement occurred");

  // Clean up.
  resolveQuery();
  UrlbarProvidersManager.unregisterProvider(provider);
  cleanup();
  sandboxCleanup();
}

/**
 * A test provider that doesn't finish `startQuery()` until `finishQueryPromise`
 * is resolved.
 */
class DelayingTestProvider extends UrlbarTestUtils.TestProvider {
  finishQueryPromise = null;
  async startQuery(context, addCallback) {
    for (let result of this._results) {
      addCallback(this, result);
    }
    await this.finishQueryPromise;
  }
}
