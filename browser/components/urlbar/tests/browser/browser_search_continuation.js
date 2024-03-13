/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests how trending and recent searches work together.
 */

const CONFIG_DEFAULT = [
  {
    webExtension: { id: "basic@search.mozilla.org" },
    urls: {
      trending: {
        fullPath:
          "https://example.com/browser/browser/components/search/test/browser/trendingSuggestionEngine.sjs",
        query: "",
      },
    },
    appliesTo: [{ included: { everywhere: true } }],
    default: "yes",
  },
];

const CONFIG_V2 = [
  {
    recordType: "engine",
    identifier: "basic",
    base: {
      name: "basic",
      urls: {
        search: {
          base: "https://example.com",
          searchTermParamName: "q",
        },
        trending: {
          base: "https://example.com/browser/browser/components/search/test/browser/trendingSuggestionEngine.sjs",
          method: "GET",
        },
      },
      aliases: ["basic"],
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "private",
    base: {
      name: "private",
      urls: {
        search: {
          base: "https://example.com",
          searchTermParamName: "q",
        },
        suggestions: {
          base: "https://example.com",
          method: "GET",
          searchTermParamName: "search",
        },
      },
      aliases: ["private"],
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
    ],
  },
  {
    recordType: "defaultEngines",
    globalDefault: "basic",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.suggest.trending", true],
      ["browser.urlbar.maxRichResults", 3],
      ["browser.urlbar.trending.featureGate", true],
      ["browser.urlbar.trending.requireSearchMode", false],
      ["browser.urlbar.suggest.recentsearches", true],
      ["browser.urlbar.recentsearches.featureGate", true],
      [
        "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
        false,
      ],
    ],
  });

  await UrlbarTestUtils.formHistory.clear();
  await SearchTestUtils.setupTestEngines(
    "search-engines",
    SearchUtils.newSearchConfigEnabled ? CONFIG_V2 : CONFIG_DEFAULT
  );

  registerCleanupFunction(async () => {
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async function test_trending_results() {
  await check_results([
    "SearchSuggestions",
    "SearchSuggestions",
    "SearchSuggestions",
  ]);
  await doSearch("Testing 1");
  await check_results([
    "RecentSearches",
    "SearchSuggestions",
    "SearchSuggestions",
  ]);
  await doSearch("Testing 2");
  await check_results([
    "RecentSearches",
    "RecentSearches",
    "SearchSuggestions",
  ]);
  await doSearch("Testing 3");
  await check_results(["RecentSearches", "RecentSearches", "RecentSearches"]);
});

async function check_results(results) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
    waitForFocus: SimpleTest.waitForFocus,
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    results.length,
    "We matched the expected number of results"
  );

  for (let i = 0; i < results.length; i++) {
    let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.equal(result.providerName, results[i]);
  }

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
}

async function doSearch(search) {
  info("Perform a search that will be added to search history.");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "data:text/html,"
  );
  let browserLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: search,
    waitForFocus: SimpleTest.waitForFocus,
  });

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
  });
  await browserLoaded;

  await BrowserTestUtils.removeTab(tab);
}
