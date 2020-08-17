/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests search suggestions in search mode.
 */

const DEFAULT_ENGINE_NAME = "Test";
const SUGGESTIONS_ENGINE_NAME = "searchSuggestionEngine.xml";

let suggestionsEngine;
let defaultEngine;

add_task(async function setup() {
  suggestionsEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + SUGGESTIONS_ENGINE_NAME
  );

  let oldDefaultEngine = await Services.search.getDefault();
  defaultEngine = await Services.search.addEngineWithDetails(
    DEFAULT_ENGINE_NAME,
    {
      template: "http://example.com/?search={searchTerms}",
    }
  );
  await Services.search.setDefault(defaultEngine);
  await Services.search.moveEngine(suggestionsEngine, 0);

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  // Add some form history.
  await UrlbarTestUtils.formHistory.add([
    { value: "hello formHistory 1", source: suggestionsEngine.name },
    { value: "hello formHistory 2", source: suggestionsEngine.name },
    { value: "hello formHistory global" },
    { value: "hello formHistory other", source: "other engine" },
  ]);

  registerCleanupFunction(async () => {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(defaultEngine);
    await UrlbarTestUtils.formHistory.clear();
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", false],
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });
});

add_task(async function emptySearch() {
  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, "", "Urlbar value should be cleared.");
    // For the empty search case, we expect to get the form history relative to
    // the picked engine and no heuristic.
    await checkResults([
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        searchParams: {
          isSearchHistory: true,
          suggestion: "hello formHistory 1",
          engine: suggestionsEngine.name,
        },
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        searchParams: {
          isSearchHistory: true,
          suggestion: "hello formHistory 2",
          engine: suggestionsEngine.name,
        },
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

add_task(async function nonEmptySearch() {
  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
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
          isSearchHistory: false,
          engine: suggestionsEngine.name,
        },
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        searchParams: {
          isSearchHistory: true,
          suggestion: "hello formHistory 1",
          engine: suggestionsEngine.name,
        },
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        searchParams: {
          isSearchHistory: true,
          suggestion: "hello formHistory 2",
          engine: suggestionsEngine.name,
        },
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          isSearchHistory: false,
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
          isSearchHistory: false,
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
  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
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
          isSearchHistory: false,
          engine: suggestionsEngine.name,
        },
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          isSearchHistory: false,
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
          isSearchHistory: false,
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
  // URLs with the same host as the search engine.
  let query = "ciao";
  await PlacesTestUtils.addVisits([
    `http://mochi.test/${query}`,
    `http://mochi.test/${query}1`,
  ]);

  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: query,
    });

    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, query, "Urlbar value should be set.");
    await checkResults([
      {
        heuristic: true,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          isSearchHistory: false,
          engine: suggestionsEngine.name,
        },
      },
      {
        heuristic: false,
        type: UrlbarUtils.RESULT_TYPE.SEARCH,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        searchParams: {
          query,
          isSearchHistory: false,
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
          isSearchHistory: false,
          suggestion: `${query}bar`,
          engine: suggestionsEngine.name,
        },
      },
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
  });

  await PlacesUtils.history.clear();
});

add_task(async function nonEmptySearch_url() {
  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
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
          isSearchHistory: false,
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
