/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Main tests for SearchSERPTelemetry - general engine visiting and link
 * clicking on about pages.
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

      BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, page);
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
      await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
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
