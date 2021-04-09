/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Main tests for SearchSERPTelemetry - general engine visiting and link clicking.
 */

"use strict";

const { BrowserSearchTelemetry } = ChromeUtils.import(
  "resource:///modules/BrowserSearchTelemetry.jsm"
);
const { SearchSERPTelemetry } = ChromeUtils.import(
  "resource:///modules/SearchSERPTelemetry.jsm"
);
const { UrlbarTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlbarTestUtils.jsm"
);
const { SearchTestUtils } = ChromeUtils.import(
  "resource://testing-common/SearchTestUtils.jsm"
);

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp: /^https:\/\/example.com\/browser\/browser\/components\/search\/test\/browser\/searchTelemetry(?:Ad)?.html/,
    queryParamName: "s",
    codeParamName: "abc",
    codePrefixes: ["ff"],
    followOnParamNames: ["a"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad2?/],
  },
];

function getPageUrl(useAdPage = false) {
  let page = useAdPage ? "searchTelemetryAd.html" : "searchTelemetry.html";
  return `https://example.com/browser/browser/components/search/test/browser/${page}`;
}

/**
 * Returns the index of the first search suggestion in the urlbar results.
 *
 * @returns {number} An index, or -1 if there are no search suggestions.
 */
async function getFirstSuggestionIndex() {
  const matchCount = UrlbarTestUtils.getResultCount(window);
  for (let i = 0; i < matchCount; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.searchParams.suggestion
    ) {
      return i;
    }
  }
  return -1;
}

// sharedData messages are only passed to the child on idle. Therefore
// we wait for a few idles to try and ensure the messages have been able
// to be passed across and handled.
async function waitForIdle() {
  for (let i = 0; i < 10; i++) {
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));
  }
}

SearchTestUtils.init(this);
UrlbarTestUtils.init(this);

add_task(async function setup() {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  Services.prefs.setBoolPref("browser.search.log", true);

  let currentEngineName = (await Services.search.getDefault()).name;

  await SearchTestUtils.installSearchExtension({
    search_url: getPageUrl(true),
    search_url_get_params: "s={searchTerms}&abc=ff",
    suggest_url:
      "https://example.com/browser/browser/components/search/test/browser/searchSuggestionEngine.sjs",
    suggest_url_get_params: "query={searchTerms}",
  });
  let engine1 = Services.search.getEngineByName("Example");

  await Services.search.setDefault(engine1);

  await gCUITestUtils.addSearchBar();

  registerCleanupFunction(async () => {
    gCUITestUtils.removeSearchBar();
    Services.prefs.clearUserPref("browser.search.log");
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    Services.telemetry.clearScalars();
    await Services.search.setDefault(
      Services.search.getEngineByName(currentEngineName)
    );
  });
});

async function track_ad_click(
  expectedHistogramSource,
  expectedScalarSource,
  searchAdsFn,
  cleanupFn
) {
  searchCounts.clear();
  Services.telemetry.clearScalars();

  let expectedContentScalarKey = "example:tagged:ff";
  let expectedScalarKeyOld = "example:sap";
  let expectedScalarKey = "example:tagged";
  let expectedHistogramKey = "example.in-content:sap:ff";
  let expectedHistogramSAPSourceKey = `other-Example.${expectedHistogramSource}`;
  let expectedContentScalar = `browser.search.content.${expectedScalarSource}`;
  let expectedWithAdsScalar = `browser.search.withads.${expectedScalarSource}`;
  let expectedAdClicksScalar = `browser.search.adclicks.${expectedScalarSource}`;

  let tab = await searchAdsFn();

  await assertSearchSourcesTelemetry(
    {
      [expectedHistogramKey]: 1,
      [expectedHistogramSAPSourceKey]: 1,
    },
    {
      [expectedContentScalar]: { [expectedContentScalarKey]: 1 },
      "browser.search.with_ads": { [expectedScalarKeyOld]: 1 },
      [expectedWithAdsScalar]: { [expectedScalarKey]: 1 },
    }
  );

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.getElementById("ad1").click();
  });
  await pageLoadPromise;
  await promiseWaitForAdLinkCheck();

  await assertSearchSourcesTelemetry(
    {
      [expectedHistogramKey]: 1,
      [expectedHistogramSAPSourceKey]: 1,
    },
    {
      [expectedContentScalar]: { [expectedContentScalarKey]: 1 },
      "browser.search.with_ads": { [expectedScalarKeyOld]: 1 },
      [expectedWithAdsScalar]: { [expectedScalarKey]: 1 },
      "browser.search.ad_clicks": { [expectedScalarKeyOld]: 1 },
      [expectedAdClicksScalar]: { [expectedScalarKey]: 1 },
    }
  );

  await cleanupFn();
}

add_task(async function test_source_urlbar() {
  let tab;
  await track_ad_click(
    "urlbar",
    "urlbar",
    async () => {
      tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "searchSuggestion",
      });
      let idx = await getFirstSuggestionIndex();
      Assert.greaterOrEqual(idx, 0, "there should be a first suggestion");
      let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
      while (idx--) {
        EventUtils.sendKey("down");
      }
      EventUtils.sendKey("return");
      await loadPromise;
      return tab;
    },
    async () => {
      BrowserTestUtils.removeTab(tab);
    }
  );
});

add_task(async function test_source_searchbar() {
  let tab;
  await track_ad_click(
    "searchbar",
    "searchbar",
    async () => {
      tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

      let sb = BrowserSearch.searchBar;
      // Write the search query in the searchbar.
      sb.focus();
      sb.value = "searchSuggestion";
      sb.textbox.controller.startSearch("searchSuggestion");
      // Wait for the popup to show.
      await BrowserTestUtils.waitForEvent(sb.textbox.popup, "popupshown");
      // And then for the search to complete.
      await BrowserTestUtils.waitForCondition(
        () =>
          sb.textbox.controller.searchStatus >=
          Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH,
        "The search in the searchbar must complete."
      );

      let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
      EventUtils.synthesizeKey("KEY_Enter");
      await loadPromise;
      return tab;
    },
    async () => {
      BrowserTestUtils.removeTab(tab);
    }
  );
});

async function checkAboutPage(
  page,
  expectedHistogramSource,
  expectedScalarSource
) {
  let tab;
  await track_ad_click(
    expectedHistogramSource,
    expectedScalarSource,
    async () => {
      tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

      BrowserTestUtils.loadURI(tab.linkedBrowser, page);
      await BrowserTestUtils.browserStopped(tab.linkedBrowser, page);

      // Wait for the full load.
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
            false,
          ],
        ],
      });
      await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
        await ContentTaskUtils.waitForCondition(
          () => content.wrappedJSObject.gContentSearchController.defaultEngine
        );
      });

      let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
      await typeInSearchField(
        tab.linkedBrowser,
        "test query",
        "newtab-search-text"
      );
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, tab.linkedBrowser);
      await p;
      return tab;
    },
    async () => {
      BrowserTestUtils.removeTab(tab);
      await SpecialPowers.popPrefEnv();
    }
  );
}

add_task(async function test_source_about_home() {
  await checkAboutPage("about:home", "abouthome", "about_home");
});

add_task(async function test_source_about_newtab() {
  await checkAboutPage("about:newtab", "newtab", "about_newtab");
});

add_task(async function test_source_system() {
  let tab;
  await track_ad_click(
    "system",
    "system",
    async () => {
      tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

      let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);

      // This is not quite the same as calling from the commandline, but close
      // enough for this test.
      BrowserSearch.loadSearchFromCommandLine(
        "searchSuggestion",
        false,
        Services.scriptSecurityManager.getSystemPrincipal(),
        gBrowser.selectedBrowser.csp
      );

      await loadPromise;
      return tab;
    },
    async () => {
      BrowserTestUtils.removeTab(tab);
    }
  );
});

add_task(async function test_source_webextension() {
  /* global browser */
  async function background(SEARCH_TERM) {
    // Search with no tabId
    browser.search.search({ query: "searchSuggestion", engine: "Example" });
  }

  let searchExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["search", "tabs"],
    },
    background,
    useAddonManager: "temporary",
  });

  let tab;
  await track_ad_click(
    "webextension",
    "webextension",
    async () => {
      let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

      await searchExtension.startup();

      return (tab = await tabPromise);
    },
    async () => {
      await searchExtension.unload();
      BrowserTestUtils.removeTab(tab);
    }
  );
});
