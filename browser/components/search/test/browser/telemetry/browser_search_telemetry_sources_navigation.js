/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Main tests for SearchSERPTelemetry - general engine visiting and link clicking.
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry(?:Ad)?.html/,
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

let tab;

add_setup(async function () {
  searchCounts.clear();
  Services.telemetry.clearScalars();

  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
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
        "https://example.com/browser/browser/components/search/test/browser/searchSuggestionEngine.sjs",
      suggest_url_get_params: "query={searchTerms}",
    },
    { setAsDefault: true }
  );

  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
  });
});

// These tests are consecutive and intentionally build on the results of the
// previous test.

async function loadSearchPage() {
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
}

add_task(async function test_search() {
  Services.fog.testResetFOG();
  // Load a page via the address bar.
  await loadSearchPage();
  await waitForPageWithAdImpressions();

  await assertSearchSourcesTelemetry(
    {
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
      },
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
});

add_task(async function test_reload() {
  let adImpressionPromise = waitForPageWithAdImpressions();
  let promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await promise;
  await adImpressionPromise;

  await assertSearchSourcesTelemetry(
    {
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.content.reload": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.withads.reload": { "example:tagged": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
      },
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "2",
          ads_visible: "2",
          ads_hidden: "0",
        },
      ],
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
      },
    },
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "reload",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
      },
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

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#ad1", {}, tab.linkedBrowser);
  await pageLoadPromise;

  await assertSearchSourcesTelemetry(
    {
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.content.reload": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.withads.reload": { "example:tagged": 1 },
      "browser.search.adclicks.reload": { "example:tagged": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
      },
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "2",
          ads_visible: "2",
          ads_hidden: "0",
        },
      ],
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
      },
    },
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "reload",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
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
});

let searchUrl;

add_task(async function test_fresh_search() {
  resetTelemetry();

  // Load a page via the address bar.
  let adImpressionPromise = waitForPageWithAdImpressions();
  await loadSearchPage();
  await adImpressionPromise;

  searchUrl = tab.linkedBrowser.url;

  await assertSearchSourcesTelemetry(
    {
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
      },
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
});

add_task(async function test_click_ad() {
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#ad1", {}, tab.linkedBrowser);
  await pageLoadPromise;

  await assertSearchSourcesTelemetry(
    {
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.adclicks.urlbar": { "example:tagged": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
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
});

add_task(async function test_go_back() {
  let adImpressionPromise = waitForPageWithAdImpressions();
  let promise = BrowserTestUtils.waitForLocationChange(gBrowser, searchUrl);
  tab.linkedBrowser.goBack();
  await promise;
  await adImpressionPromise;

  await assertSearchSourcesTelemetry(
    {
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.content.tabhistory": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.withads.tabhistory": { "example:tagged": 1 },
      "browser.search.adclicks.urlbar": { "example:tagged": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
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
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "tabhistory",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
      },
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

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#ad1", {}, tab.linkedBrowser);
  await pageLoadPromise;

  await assertSearchSourcesTelemetry(
    {
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.content.tabhistory": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.withads.tabhistory": { "example:tagged": 1 },
      "browser.search.adclicks.urlbar": { "example:tagged": 1 },
      "browser.search.adclicks.tabhistory": { "example:tagged": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
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
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "tabhistory",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
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
});

// Conduct a search from the Urlbar with showSearchTerms enabled.
add_task(async function test_fresh_search_with_urlbar_persisted() {
  resetTelemetry();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.showSearchTerms.featureGate", true],
      ["browser.urlbar.tipShownCount.searchTip_persist", 999],
    ],
  });

  // Load a SERP once in order to show the search term in the Urlbar.
  let adImpressionPromise = waitForPageWithAdImpressions();
  await loadSearchPage();
  await adImpressionPromise;
  await assertSearchSourcesTelemetry(
    {
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
      },
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

  // Do another search from the context of the default SERP.
  adImpressionPromise = waitForPageWithAdImpressions();
  await loadSearchPage();
  await adImpressionPromise;
  await assertSearchSourcesTelemetry(
    {
      "other-Example.urlbar": 1,
      "other-Example.urlbar-persisted": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.content.urlbar_persisted": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar_persisted": { "example:tagged": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
      },
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "2",
          ads_visible: "2",
          ads_hidden: "0",
        },
      ],
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
      },
    },
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar_persisted",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
      },
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

  // Click on an ad.
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#ad1", {}, tab.linkedBrowser);

  await pageLoadPromise;
  await assertSearchSourcesTelemetry(
    {
      "other-Example.urlbar": 1,
      "other-Example.urlbar-persisted": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.content.urlbar_persisted": { "example:tagged:ff": 1 },
      "browser.search.withads.urlbar_persisted": { "example:tagged": 1 },
      "browser.search.adclicks.urlbar_persisted": { "example:tagged": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
      },
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "2",
          ads_visible: "2",
          ads_hidden: "0",
        },
      ],
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
      },
    },
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "urlbar_persisted",
        shopping_tab_displayed: "false",
        is_shopping_page: "false",
        is_private: "false",
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

  await SpecialPowers.popPrefEnv();
});
