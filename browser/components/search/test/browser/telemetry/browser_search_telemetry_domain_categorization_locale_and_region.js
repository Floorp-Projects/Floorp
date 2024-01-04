/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * These tests check that changing the locale and region actually results in
 * reporting the correct changes. Other tests that include the locale and region
 * just report the locale/region used by the test.
 */

ChromeUtils.defineESModuleGetters(this, {
  Region: "resource://gre/modules/Region.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
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

const originalHomeRegion = Region.home;
const originalCurrentRegion = Region.current;

const originalAvailableLocales = Services.locale.availableLocales;
const originalRequestedLocales = Services.locale.requestedLocales;

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();

  let promise = waitForDomainToCategoriesUpdate();
  await insertRecordIntoCollectionAndSync();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.serpEventTelemetryCategorization.enabled", true]],
  });
  await promise;

  info("Change region to DE.");
  Region._setHomeRegion("DE", false);
  Assert.equal(Region.home, "DE", "Region");

  info("Change locale to DE.");
  Services.locale.availableLocales = ["de"];
  Services.locale.requestedLocales = ["de"];
  Assert.equal(Services.locale.appLocaleAsBCP47, "de", "Locale");

  registerCleanupFunction(async () => {
    Region._setHomeRegion(originalHomeRegion);
    Region._setCurrentRegion(originalCurrentRegion);

    Services.locale.availableLocales = originalAvailableLocales;
    Services.locale.requestedLocales = originalRequestedLocales;

    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

add_task(
  async function test_categorize_page_with_different_region_and_locale() {
    resetTelemetry();

    let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
    info("Load a SERP with organic and sponsored results.");
    let promise = waitForPageWithCategorizedDomains();
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    await promise;

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
        locale: "de",
        region: "DE",
        partner_code: "ff",
        provider: "example",
        tagged: "true",
        num_ads_clicked: "0",
        num_ads_visible: "2",
      },
    ]);
  }
);
