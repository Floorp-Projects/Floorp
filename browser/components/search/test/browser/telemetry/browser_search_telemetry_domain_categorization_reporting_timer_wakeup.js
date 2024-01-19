/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * These tests check that we report the SERP categorization upon waking a
 * computer and enough time has passed.
 */

ChromeUtils.defineESModuleGetters(this, {
  CATEGORIZATION_SETTINGS: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPCategorizationEventScheduler:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
  TELEMETRY_CATEGORIZATION_KEY:
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
  let oldWakeTimeout = CATEGORIZATION_SETTINGS.WAKE_TIMEOUT_MS;

  // Use a sane timeout.
  CATEGORIZATION_SETTINGS.WAKE_TIMEOUT_MS = 100;

  SearchTestUtils.useMockIdleService();
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
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
    CATEGORIZATION_SETTINGS.WAKE_TIMEOUT_MS = oldWakeTimeout;
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

add_task(async function test_categorize_serp_and_sleep() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a SERP with organic and sponsored results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  assertCategorizationValues([]);

  info("Wait enough between the categorization and the sleep timeout.");
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 200));

  info("Simulate a wake notification.");
  promise = waitForAllCategorizedEvents();
  SearchTestUtils.idleService._fireObservers("wake_notification");
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
      app_version: APP_VERSION,
      channel: CHANNEL,
      locale: LOCALE,
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
  SearchTestUtils.idleService._fireObservers("wake_notification");
  await BrowserTestUtils.removeTab(tab);

  assertCategorizationValues([]);
});

add_task(async function test_categorize_serp_and_sleep_not_long_enough() {
  resetTelemetry();

  // Use a really long timeout.
  CATEGORIZATION_SETTINGS.WAKE_TIMEOUT_MS = 500_000;

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a SERP with organic and sponsored results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  assertCategorizationValues([]);

  info("Wait as long as the previous test.");
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 200));
  assertCategorizationValues([]);

  info("Simulate a wake notification.");
  SearchTestUtils.idleService._fireObservers("wake_notification");
  assertCategorizationValues([]);

  // Closing the tab should record the telemetry.
  await BrowserTestUtils.removeTab(tab);
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
      app_version: APP_VERSION,
      channel: CHANNEL,
      locale: LOCALE,
      region: REGION,
      partner_code: "ff",
      provider: "example",
      tagged: "true",
      num_ads_clicked: "0",
      num_ads_visible: "2",
    },
  ]);
});
