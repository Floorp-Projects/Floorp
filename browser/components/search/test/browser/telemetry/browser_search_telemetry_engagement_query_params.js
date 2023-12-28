/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests load SERPs and check that query params that are changed either
 * by the browser or in the page after click are still properly recognized
 * as ads.
 *
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        included: {
          parent: {
            selector: ".moz_ad",
          },
          children: [
            {
              selector: ".multi-col",
              type: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
            },
          ],
        },
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
    set: [["browser.search.serpEventTelemetry.enabled", true]],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
  });
});

// Baseline test clicking on either link properly categorizes both properly.
add_task(async function test_click_links() {
  let url = getSERPUrl("searchTelemetryAd_components_query_parameters.html");

  info("Load SERP.");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  info("Click on ad link.");
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a#ad_link",
    {},
    tab.linkedBrowser
  );
  await pageLoadPromise;

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
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
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  info("Load SERP again.");
  BrowserTestUtils.startLoadingURIString(gBrowser, url);
  pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await waitForPageWithAdImpressions();

  info("Click on site link.");
  pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a#ad_sitelink",
    {},
    tab.linkedBrowser
  );
  await pageLoadPromise;

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
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
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
        },
      ],
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});

add_task(async function test_click_link_with_more_parameters() {
  let url = getSERPUrl("searchTelemetryAd_components_query_parameters.html");

  info("Load SERP.");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  info("After ad impressions, add query parameters to DOM element.");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let el = content.document.getElementById("ad_sitelink");
    let domUrl = new URL(el.href);
    domUrl.searchParams.set("example", "param");
    el.setAttribute("href", domUrl.toString());
  });

  info("Click on site link.");
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a#ad_sitelink",
    {},
    tab.linkedBrowser
  );
  await pageLoadPromise;

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
        },
      ],
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});

add_task(async function test_click_link_with_fewer_parameters() {
  let url = getSERPUrl("searchTelemetryAd_components_query_parameters.html");

  info("Load SERP.");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  info("After ad impressions, remove a query parameter from a DOM element.");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let el = content.document.getElementById("ad_sitelink");
    let domUrl = new URL(el.href);
    domUrl.searchParams.delete("foo");
    el.setAttribute("href", domUrl.toString());
  });

  info("Click on site link.");
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a#ad_sitelink",
    {},
    tab.linkedBrowser
  );
  await pageLoadPromise;

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
        },
      ],
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});

add_task(async function test_click_link_with_reordered_parameters() {
  let url = getSERPUrl("searchTelemetryAd_components_query_parameters.html");

  info("Load SERP.");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  info("After ad impressions, re-sort the query params.");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let el = content.document.getElementById("ad_sitelink");
    let domUrl = new URL(el.href);
    domUrl.searchParams.sort();
    el.setAttribute("href", domUrl.toString());
  });

  info("Click on site link.");
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a#ad_sitelink",
    {},
    tab.linkedBrowser
  );
  await pageLoadPromise;

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
        },
      ],
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});
