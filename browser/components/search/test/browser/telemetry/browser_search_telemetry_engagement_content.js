/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests load a SERP that has multiple ways of refining a search term
 * within content, or moving it into another search engine. It is also common
 * for providers to remove tracking params.
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_searchbox_with_content.html/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_searchbox_with_content_redirect.html/,
    ],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
    shoppingTab: {
      selector: "nav a",
      regexp: "&page=shopping",
      inspectRegexpInSERP: true,
    },
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
        included: {
          parent: {
            selector: "form",
          },
          children: [
            {
              selector: "input",
            },
          ],
          related: {
            selector: "div",
          },
        },
        topDown: true,
        nonAd: true,
      },
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        included: {
          parent: {
            selector: ".refined-search-buttons",
          },
          children: [
            {
              selector: "a",
            },
          ],
        },
        topDown: true,
        nonAd: true,
      },
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
];

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.log", true],
      ["browser.search.serpEventTelemetry.enabled", true],
    ],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
  });
});

// "Tabs" are considered to be links the navigation of a SERP. Their hrefs
// may look similar to a search page, including related searches.
add_task(async function test_click_tab() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#images",
    {},
    tab.linkedBrowser
  );
  await pageLoadPromise;

  await TestUtils.waitForCondition(() => {
    return Glean.serp.impression?.testGetValue()?.length == 2;
  }, "Should have two impressions.");

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

// Ensure that shopping links on a page with many non-ad link regular
// expressions doesn't get confused for a non-ads link.
add_task(async function test_click_shopping() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#shopping",
    {},
    tab.linkedBrowser
  );
  await pageLoadPromise;

  await TestUtils.waitForCondition(() => {
    return Glean.serp.impression?.testGetValue()?.length == 2;
  }, "Should have two impressions.");

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.SHOPPING_TAB,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
        source: "unknown",
        is_shopping_page: "true",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_click_related_search_in_new_tab() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_searchbox_with_content.html?s=test+one+two+three";

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, targetUrl, true);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#related-new-tab",
    {},
    tab.linkedBrowser
  );
  let tab2 = await tabPromise;

  await TestUtils.waitForCondition(() => {
    return Glean.serp.impression?.testGetValue()?.length == 2;
  }, "Should have two impressions.");

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
        source: "opened_in_new_tab",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});

// We consider regular expressions in nonAdsLinkRegexps and searchPageRegexp
// as valid non ads links when recording an engagement event.
add_task(async function test_click_redirect_search_in_newtab() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_searchbox_with_content.html?s=test+one+two+three";

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, targetUrl, true);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#related-redirect",
    {},
    tab.linkedBrowser
  );
  let tab2 = await tabPromise;

  await waitForPageWithAdImpressions();

  await TestUtils.waitForCondition(() => {
    return Glean.serp.impression.testGetValue()?.length == 2;
  }, "Should have two impressions.");

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
        source: "opened_in_new_tab",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});

// Ensure if a user does a search that uses one of the in-content sources,
// we clear the cached source value.
add_task(async function test_content_source_reset() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  // Do a text search to trigger a defined target.
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "form input",
    {},
    tab.linkedBrowser
  );
  EventUtils.synthesizeKey("KEY_Enter");
  await pageLoadPromise;

  // Click on a related search that will load within the same page and should
  // have an unknown target.
  await waitForPageWithAdImpressions();
  pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#related-in-page",
    {},
    tab.linkedBrowser
  );
  await pageLoadPromise;

  await TestUtils.waitForCondition(() => {
    return Glean.serp.impression.testGetValue()?.length == 3;
  }, "Should have three impressions.");

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
        },
        {
          action: SearchSERPTelemetryUtils.ACTIONS.SUBMITTED,
          target: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "follow_on_from_refine_on_incontent_search",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

// This test also deliberately includes an anchor with a reserved character in
// the href that gets parsed on page load. This is because when the URL is
// requested and observed in the network process, it is converted into a
// percent encoded string, so we want to ensure we're categorizing the
// component properly. This can happen with refinement buttons.
add_task(async function test_click_refinement_button() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_searchbox_with_content.html?s=test%27s";

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    targetUrl
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#refined-search-button",
    {},
    tab.linkedBrowser
  );
  await pageLoadPromise;

  await TestUtils.waitForCondition(() => {
    return Glean.serp.impression.testGetValue()?.length == 2;
  }, "Should have two impressions.");

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
        source: "follow_on_from_refine_on_SERP",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});
