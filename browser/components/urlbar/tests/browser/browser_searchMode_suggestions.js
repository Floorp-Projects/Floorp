/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests search suggestions in search mode.
 */

const TEST_QUERY = "hello";
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
        isSearchHistory: true,
        suggestion: "hello formHistory 1",
      },
      {
        isSearchHistory: true,
        suggestion: "hello formHistory 2",
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
  });
});

add_task(async function nonEmptySearch() {
  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "hello",
    });
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, "hello", "Urlbar value should be set.");
    // We expect to get the heuristic and all the suggestions.
    await checkResults([
      {
        isSearchHistory: false,
        suggestion: undefined,
      },
      {
        isSearchHistory: true,
        suggestion: "hello formHistory 1",
      },
      {
        isSearchHistory: true,
        suggestion: "hello formHistory 2",
      },
      {
        isSearchHistory: false,
        suggestion: `${TEST_QUERY}foo`,
      },
      {
        isSearchHistory: false,
        suggestion: `${TEST_QUERY}bar`,
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
  });
});

add_task(async function nonEmptySearch_nonMatching() {
  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "ciao",
    });
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(gURLBar.value, "ciao", "Urlbar value should be set.");
    // We expect to get the heuristic and the remote suggestions since the local
    // ones don't match.
    await checkResults([
      {
        isSearchHistory: false,
        suggestion: undefined,
      },
      {
        isSearchHistory: false,
        suggestion: "ciaofoo",
      },
      {
        isSearchHistory: false,
        suggestion: "ciaobar",
      },
    ]);

    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
  });
});

async function checkResults(resultsDetails) {
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultsDetails.length,
    "Check results count."
  );
  for (let i = 0; i < resultsDetails.length; ++i) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.equal(
      result.searchParams.engine,
      suggestionsEngine.name,
      "It should be a search result for our suggestion engine."
    );
    Assert.equal(
      result.searchParams.isSearchHistory,
      resultsDetails[i].isSearchHistory,
      "Check if it should be a local suggestion result."
    );
    Assert.equal(
      result.searchParams.suggestion,
      resultsDetails[i].suggestion,
      "Check the suggestion value"
    );
  }
}
