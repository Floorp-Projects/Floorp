/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Main tests for SearchSERPTelemetry - general engine visiting and
 * link clicking with Web Extensions.
 *
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry(?:Ad)?/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    followOnParamNames: ["a"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad2?/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
];

SearchTestUtils.init(this);

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      [
        "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
        true,
      ],
      // Ensure to add search suggestion telemetry as search_suggestion not search_formhistory.
      ["browser.urlbar.maxHistoricalSearchSuggestions", 0],
      ["browser.search.serpEventTelemetry.enabled", true],
    ],
  });
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  await SearchTestUtils.installSearchExtension(
    {
      search_url: getPageUrl(true),
      search_url_get_params: "s={searchTerms}&abc=ff",
      suggest_url:
        "https://example.org/browser/browser/components/search/test/browser/searchSuggestionEngine.sjs",
      suggest_url_get_params: "query={searchTerms}",
    },
    { setAsDefault: true }
  );

  await gCUITestUtils.addSearchBar();

  registerCleanupFunction(async () => {
    gCUITestUtils.removeSearchBar();
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
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
  let expectedScalarKey = "example:tagged";
  let expectedHistogramSAPSourceKey = `other-Example.${expectedHistogramSource}`;
  let expectedContentScalar = `browser.search.content.${expectedScalarSource}`;
  let expectedWithAdsScalar = `browser.search.withads.${expectedScalarSource}`;
  let expectedAdClicksScalar = `browser.search.adclicks.${expectedScalarSource}`;

  let adImpressionPromise = waitForPageWithAdImpressions();
  let tab = await searchAdsFn();

  await assertSearchSourcesTelemetry(
    {
      [expectedHistogramSAPSourceKey]: 1,
    },
    {
      [expectedContentScalar]: { [expectedContentScalarKey]: 1 },
      [expectedWithAdsScalar]: { [expectedScalarKey]: 1 },
    }
  );

  await adImpressionPromise;

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#ad1", {}, tab.linkedBrowser);
  await pageLoadPromise;
  await promiseWaitForAdLinkCheck();

  await assertSearchSourcesTelemetry(
    {
      [expectedHistogramSAPSourceKey]: 1,
    },
    {
      [expectedContentScalar]: { [expectedContentScalarKey]: 1 },
      [expectedWithAdsScalar]: { [expectedScalarKey]: 1 },
      [expectedAdClicksScalar]: { [expectedScalarKey]: 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: expectedScalarSource,
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "2",
          ads_visible: "2",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  await cleanupFn();

  Services.fog.testResetFOG();
}

add_task(async function test_source_webextension_search() {
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

add_task(async function test_source_webextension_query() {
  async function background(SEARCH_TERM) {
    // Search with no tabId
    browser.search.query({
      text: "searchSuggestion",
      disposition: "NEW_TAB",
    });
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
