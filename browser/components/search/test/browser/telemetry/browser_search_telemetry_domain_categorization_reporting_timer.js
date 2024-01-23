/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * These tests check that we report the categorization if the SERP is loaded,
 * and the user idles. The tests also check that if we report the
 * categorization and trigger another event that could cause a reporting, we
 * don't cause more than one categorization to be reported.
 */

ChromeUtils.defineESModuleGetters(this, {
  SearchSERPCategorizationEventScheduler:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
});

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
    // The search telemetry entry responsible for targeting the specific results.
    domainExtraction: {
      ads: [
        {
          selectors: "[data-ad-domain]",
          method: "data-attribute",
          options: {
            dataAttributeKey: "adDomain",
          },
        },
        {
          selectors: ".ad",
          method: "href",
          options: {
            queryParamKey: "ad_domain",
          },
        },
      ],
      nonAds: [
        {
          selectors: "#results .organic a",
          method: "href",
        },
      ],
    },
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
];

add_setup(async function () {
  SearchTestUtils.useMockIdleService();
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);

  // On startup, the event scheduler is initialized.
  // If serpEventTelemetryCategorization is already true, the instance of the
  // class will be subscribed to to the real idle service instead of the mock
  // idle service. If it's false, toggling the preference (which happens later
  // in this setup) will initialize it.
  if (
    Services.prefs.getBoolPref(
      "browser.search.serpEventTelemetryCategorization.enabled"
    )
  ) {
    SearchSERPCategorizationEventScheduler.uninit();
    SearchSERPCategorizationEventScheduler.init();
  }
  await waitForIdle();

  let promise = waitForDomainToCategoriesUpdate();
  await insertRecordIntoCollectionAndSync();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.serpEventTelemetryCategorization.enabled", true]],
  });
  await promise;

  registerCleanupFunction(async () => {
    // The scheduler uses the mock idle service.
    SearchSERPCategorizationEventScheduler.uninit();
    SearchSERPCategorizationEventScheduler.init();
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

add_task(async function test_categorize_serp_and_wait() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a SERP with organic and sponsored results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  assertCategorizationValues([]);

  promise = waitForAllCategorizedEvents();
  SearchTestUtils.idleService._fireObservers("idle");
  await promise;
  assertCategorizationValues([
    {
      organic_category: "3",
      organic_num_domains: "1",
      organic_num_inconclusive: "0",
      organic_num_unknown: "0",
      sponsored_category: "4",
      sponsored_num_domains: "2",
      sponsored_num_inconclusive: "0",
      sponsored_num_unknown: "0",
      mappings_version: "1",
      app_version: APP_MAJOR_VERSION,
      channel: CHANNEL,
      region: REGION,
      partner_code: "ff",
      provider: "example",
      tagged: "true",
      num_ads_clicked: "0",
      num_ads_visible: "2",
    },
  ]);

  info("Ensure we don't record a duplicate of this event.");
  resetTelemetry();
  SearchTestUtils.idleService._fireObservers("idle");
  SearchTestUtils.idleService._fireObservers("active");
  await BrowserTestUtils.removeTab(tab);

  assertCategorizationValues([]);
});

add_task(async function test_categorize_serp_open_multiple_tabs() {
  resetTelemetry();

  let tabs = [];
  let expectedResults = [];
  for (let i = 0; i < 5; ++i) {
    let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
    info("Load a SERP with organic and sponsored results.");
    let promise = waitForPageWithCategorizedDomains();
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    await promise;
    tabs.push(tab);
    // Pushing expected results into a single array to avoid having a massive,
    // unreadable array.
    expectedResults.push({
      organic_category: "3",
      organic_num_domains: "1",
      organic_num_inconclusive: "0",
      organic_num_unknown: "0",
      sponsored_category: "4",
      sponsored_num_domains: "2",
      sponsored_num_inconclusive: "0",
      sponsored_num_unknown: "0",
      mappings_version: "1",
      app_version: APP_MAJOR_VERSION,
      channel: CHANNEL,
      region: REGION,
      partner_code: "ff",
      provider: "example",
      tagged: "true",
      num_ads_clicked: "0",
      num_ads_visible: "2",
    });
  }

  info("Simulate idle event and wait for results.");
  let promise = waitForAllCategorizedEvents();
  SearchTestUtils.idleService._fireObservers("idle");
  await promise;
  assertCategorizationValues(expectedResults);

  info("Ensure we don't record a duplicate of any event.");
  resetTelemetry();
  for (let tab of tabs) {
    await BrowserTestUtils.removeTab(tab);
  }
  assertCategorizationValues([]);
});

// Ensures we don't double record a categorization event if the closed the tab
// before an idle event.
add_task(async function test_categorize_serp_close_tab_and_wait() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a SERP with organic and sponsored results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  assertCategorizationValues([]);

  promise = waitForSingleCategorizedEvent();
  await BrowserTestUtils.removeTab(tab);
  await promise;

  assertCategorizationValues([
    {
      organic_category: "3",
      organic_num_domains: "1",
      organic_num_inconclusive: "0",
      organic_num_unknown: "0",
      sponsored_category: "4",
      sponsored_num_domains: "2",
      sponsored_num_inconclusive: "0",
      sponsored_num_unknown: "0",
      mappings_version: "1",
      app_version: APP_MAJOR_VERSION,
      channel: CHANNEL,
      region: REGION,
      partner_code: "ff",
      provider: "example",
      tagged: "true",
      num_ads_clicked: "0",
      num_ads_visible: "2",
    },
  ]);

  info("Ensure we don't record a duplicate of this event.");
  resetTelemetry();
  SearchTestUtils.idleService._fireObservers("idle");
  assertCategorizationValues([]);
});

add_task(async function test_categorize_serp_open_ad_and_wait() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a SERP with organic and sponsored results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  info("Open ad in new tab.");
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    ".ad",
    { button: 1 },
    tab.linkedBrowser
  );
  let tab2 = await promiseTabOpened;

  assertCategorizationValues([]);

  promise = waitForAllCategorizedEvents();
  SearchTestUtils.idleService._fireObservers("idle");
  info("Waiting for categorized events.");
  await promise;

  assertCategorizationValues([
    {
      organic_category: "3",
      organic_num_domains: "1",
      organic_num_inconclusive: "0",
      organic_num_unknown: "0",
      sponsored_category: "4",
      sponsored_num_domains: "2",
      sponsored_num_inconclusive: "0",
      sponsored_num_unknown: "0",
      mappings_version: "1",
      app_version: APP_MAJOR_VERSION,
      channel: CHANNEL,
      region: REGION,
      partner_code: "ff",
      provider: "example",
      tagged: "true",
      num_ads_clicked: "1",
      num_ads_visible: "2",
    },
  ]);

  // Clean up.
  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(tab2);
});
