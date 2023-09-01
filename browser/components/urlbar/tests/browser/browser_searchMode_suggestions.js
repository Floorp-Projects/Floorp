/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests search suggestions in search mode.
 */

const DEFAULT_ENGINE_NAME = "Test";
const SUGGESTIONS_ENGINE_NAME = "searchSuggestionEngine.xml";
const MANY_SUGGESTIONS_ENGINE_NAME = "searchSuggestionEngineMany.xml";
const MAX_RESULT_COUNT = UrlbarPrefs.get("maxRichResults");

let suggestionsEngine;
let expectedFormHistoryResults = [];

add_setup(async function () {
  suggestionsEngine = await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + SUGGESTIONS_ENGINE_NAME,
  });

  await SearchTestUtils.installSearchExtension(
    {
      name: DEFAULT_ENGINE_NAME,
      keyword: "@test",
    },
    { setAsDefault: true }
  );
  await Services.search.moveEngine(suggestionsEngine, 0);

  async function cleanup() {
    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
  }
  await cleanup();
  registerCleanupFunction(cleanup);

  // Add some form history for our test engine.
  for (let i = 0; i < MAX_RESULT_COUNT; i++) {
    let value = `hello formHistory ${i}`;
    await UrlbarTestUtils.formHistory.add([
      { value, source: suggestionsEngine.name },
    ]);
    expectedFormHistoryResults.push({
      heuristic: false,
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.HISTORY,
      searchParams: {
        suggestion: value,
        engine: suggestionsEngine.name,
      },
    });
  }

  // Add other form history.
  await UrlbarTestUtils.formHistory.add([
    { value: "hello formHistory global" },
    { value: "hello formHistory other", source: "other engine" },
  ]);

  registerCleanupFunction(async () => {
    await UrlbarTestUtils.formHistory.clear();
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", false],
      ["browser.urlbar.suggest.quickactions", false],
      ["browser.urlbar.suggest.trending", false],
    ],
  });
});

add_task(async function emptySearch() {
  await BrowserTestUtils.withNewTab("about:robots", async function (browser) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.update2.emptySearchBehavior", 2]],
    });
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, "", "Urlbar value should be cleared.");
    // For the empty search case, we expect to get the form history relative to
    // the picked engine and no heuristic.
    await checkResults(expectedFormHistoryResults);
    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);
    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function emptySearch_withRestyledHistory() {
  // URLs with the same host as the search engine.
  await PlacesTestUtils.addVisits([
    `http://mochi.test/`,
    `http://mochi.test/redirect`,
    // Should not be returned because it's a redirect target.
    {
      uri: `http://mochi.test/target`,
      transition: PlacesUtils.history.TRANSITIONS.REDIRECT_TEMPORARY,
      referrer: `http://mochi.test/redirect`,
    },
    // Can be restyled and dupes form history.
    "http://mochi.test:8888/?terms=hello+formHistory+0",
    // Can be restyled but does not dupe form history.
    "http://mochi.test:8888/?terms=ciao",
  ]);
  await BrowserTestUtils.withNewTab("about:robots", async function (browser) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.update2.emptySearchBehavior", 2]],
    });
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, "", "Urlbar value should be cleared.");
    // For the empty search case, we expect to get the form history relative to
    // the picked engine, history without redirects, and no heuristic.
    await checkResults([
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        searchParams: {
          suggestion: "ciao",
          engine: suggestionsEngine.name,
        },
      },
      ...expectedFormHistoryResults.slice(0, MAX_RESULT_COUNT - 3),
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        url: `http://mochi.test/`,
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        url: `http://mochi.test/redirect`,
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);
    await SpecialPowers.popPrefEnv();
  });

  await PlacesUtils.history.clear();
});

add_task(async function emptySearch_withRestyledHistory_noSearchHistory() {
  // URLs with the same host as the search engine.
  await PlacesTestUtils.addVisits([
    `http://mochi.test/`,
    `http://mochi.test/redirect`,
    // Can be restyled but does not dupe form history.
    "http://mochi.test:8888/?terms=ciao",
  ]);
  await BrowserTestUtils.withNewTab("about:robots", async function (browser) {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update2.emptySearchBehavior", 2],
        ["browser.urlbar.maxHistoricalSearchSuggestions", 0],
      ],
    });
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, "", "Urlbar value should be cleared.");
    // maxHistoricalSearchSuggestions == 0, so form history should not be
    // present.
    await checkResults([
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        url: `http://mochi.test/redirect`,
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        url: `http://mochi.test/`,
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);
    await SpecialPowers.popPrefEnv();
  });

  await PlacesUtils.history.clear();
});

add_task(async function emptySearch_behavior() {
  // URLs with the same host as the search engine.
  await PlacesTestUtils.addVisits([`http://mochi.test/`]);

  await BrowserTestUtils.withNewTab("about:robots", async function (browser) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.update2.emptySearchBehavior", 0]],
    });
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, "", "Urlbar value should be cleared.");
    // For the empty search case, we expect to get the form history relative to
    // the picked engine, history without redirects, and no heuristic.
    await checkResults([]);
    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });

    // We should still show history for empty searches when not in search mode.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: " ",
    });
    await checkResults([
      {
        heuristic: true,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query: " ",
          engine: DEFAULT_ENGINE_NAME,
        },
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        url: `http://mochi.test/`,
      },
    ]);
    await UrlbarTestUtils.promisePopupClose(window);
    await SpecialPowers.popPrefEnv();
  });

  await BrowserTestUtils.withNewTab("about:robots", async function (browser) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.update2.emptySearchBehavior", 1]],
    });
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, "", "Urlbar value should be cleared.");
    // For the empty search case, we expect to get the form history relative to
    // the picked engine, history without redirects, and no heuristic.
    await checkResults([...expectedFormHistoryResults]);
    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);
    await SpecialPowers.popPrefEnv();
  });

  await PlacesUtils.history.clear();
});

add_task(async function emptySearch_local() {
  await PlacesTestUtils.addVisits([`http://mochi.test/`]);

  await BrowserTestUtils.withNewTab("about:robots", async function (browser) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.update2.emptySearchBehavior", 0]],
    });
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    });
    Assert.equal(gURLBar.value, "", "Urlbar value should be cleared.");
    // Even when emptySearchBehavior is 0, we still show the user's most frecent
    // history for an empty search.
    await checkResults([
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        url: `http://mochi.test/`,
      },
    ]);
    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);
    await SpecialPowers.popPrefEnv();
  });

  await PlacesUtils.history.clear();
});

add_task(async function nonEmptySearch() {
  await BrowserTestUtils.withNewTab("about:robots", async function (browser) {
    let query = "hello";
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: query,
    });
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, query, "Urlbar value should be set.");
    // We expect to get the heuristic and all the suggestions.
    await checkResults([
      {
        heuristic: true,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          engine: suggestionsEngine.name,
        },
      },
      ...expectedFormHistoryResults.slice(0, MAX_RESULT_COUNT - 3),
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          suggestion: `${query}foo`,
          engine: suggestionsEngine.name,
        },
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          suggestion: `${query}bar`,
          engine: suggestionsEngine.name,
        },
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

add_task(async function nonEmptySearch_nonMatching() {
  await BrowserTestUtils.withNewTab("about:robots", async function (browser) {
    let query = "ciao";
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: query,
    });
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, query, "Urlbar value should be set.");
    // We expect to get the heuristic and the remote suggestions since the local
    // ones don't match.
    await checkResults([
      {
        heuristic: true,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          engine: suggestionsEngine.name,
        },
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          suggestion: `${query}foo`,
          engine: suggestionsEngine.name,
        },
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          suggestion: `${query}bar`,
          engine: suggestionsEngine.name,
        },
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

add_task(async function nonEmptySearch_withHistory() {
  let manySuggestionsEngine = await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + MANY_SUGGESTIONS_ENGINE_NAME,
  });
  // URLs with the same host as the search engine.
  let query = "ciao";
  await PlacesTestUtils.addVisits([
    `http://mochi.test/${query}`,
    `http://mochi.test/${query}1`,
    // Should not be returned because it has a different host, even if it
    // matches the host in the path.
    `http://example.com/mochi.test/${query}`,
  ]);

  function makeSuggestionResult(suffix) {
    return {
      heuristic: false,
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      searchParams: {
        query,
        suggestion: `${query}${suffix}`,
        engine: manySuggestionsEngine.name,
      },
    };
  }

  await BrowserTestUtils.withNewTab("about:robots", async function (browser) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: query,
    });
    await UrlbarTestUtils.enterSearchMode(window, {
      engineName: manySuggestionsEngine.name,
    });
    Assert.equal(gURLBar.value, query, "Urlbar value should be set.");

    await checkResults([
      {
        heuristic: true,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          engine: manySuggestionsEngine.name,
        },
      },
      makeSuggestionResult("foo"),
      makeSuggestionResult("bar"),
      makeSuggestionResult("1"),
      makeSuggestionResult("2"),
      makeSuggestionResult("3"),
      makeSuggestionResult("4"),
      makeSuggestionResult("5"),
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        url: `http://mochi.test/${query}1`,
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        url: `http://mochi.test/${query}`,
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);

    info("Test again with history before suggestions");
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.showSearchSuggestionsFirst", false]],
    });

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: query,
    });
    await UrlbarTestUtils.enterSearchMode(window, {
      engineName: manySuggestionsEngine.name,
    });
    Assert.equal(gURLBar.value, query, "Urlbar value should be set.");

    await checkResults([
      {
        heuristic: true,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          engine: manySuggestionsEngine.name,
        },
      },
      makeSuggestionResult("foo"),
      makeSuggestionResult("bar"),
      makeSuggestionResult("1"),
      makeSuggestionResult("2"),
      makeSuggestionResult("3"),
      makeSuggestionResult("4"),
      makeSuggestionResult("5"),
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        url: `http://mochi.test/${query}1`,
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        url: `http://mochi.test/${query}`,
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);
    await SpecialPowers.popPrefEnv();
  });

  await PlacesUtils.history.clear();
});

add_task(async function nonEmptySearch_url() {
  await BrowserTestUtils.withNewTab("about:robots", async function (browser) {
    let query = "http://www.example.com/";
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: query,
    });
    await UrlbarTestUtils.enterSearchMode(window);

    // The heuristic result for a search that's a valid URL should be a search
    // result, not a URL result.
    await checkResults([
      {
        heuristic: true,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          engine: suggestionsEngine.name,
        },
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

async function checkResults(expectedResults) {
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    expectedResults.length,
    "Check results count."
  );
  for (let i = 0; i < expectedResults.length; ++i) {
    info(`Checking result at index ${i}`);
    let expected = expectedResults[i];
    let actual = await UrlbarTestUtils.getDetailsOfResultAt(window, i);

    // Check each property defined in the expected result against the property
    // in the actual result.
    for (let key of Object.keys(expected)) {
      // For searchParams, remove undefined properties in the actual result so
      // that the expected result doesn't need to include them.
      if (key == "searchParams") {
        let actualSearchParams = actual.searchParams;
        for (let spKey of Object.keys(actualSearchParams)) {
          if (actualSearchParams[spKey] === undefined) {
            delete actualSearchParams[spKey];
          }
        }
      }
      Assert.deepEqual(
        actual[key],
        expected[key],
        `${key} should match at result index ${i}.`
      );
    }
  }
}
