/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test to verify we can toggle the Glean SERP event telemetry for SERP
// categorization feature via a Nimbus variable.

const lazy = {};
const TELEMETRY_PREF =
  "browser.search.serpEventTelemetryCategorization.enabled";

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  SearchSERPDomainToCategoriesMap:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serpEventsCategorizationEnabled",
  TELEMETRY_PREF,
  false
);

// This is required to trigger and properly categorize a SERP.
const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [/^https:\/\/example.com/],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
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
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  await insertRecordIntoCollectionAndSync();
  // If the categorization preference is enabled, we should also wait for the
  // sync event to update the domain to categories map.
  if (lazy.serpEventsCategorizationEnabled) {
    await waitForDomainToCategoriesUpdate();
  }

  registerCleanupFunction(async () => {
    Services.telemetry.canRecordExtended = oldCanRecord;
    await SpecialPowers.popPrefEnv();
    resetTelemetry();
  });
});

add_task(async function test_enable_experiment_when_pref_is_not_enabled() {
  let prefBranch = Services.prefs.getDefaultBranch("");
  let originalPrefValue = prefBranch.getBoolPref(TELEMETRY_PREF);

  // Ensure the build being tested has the preference value as false.
  // Changing the preference in the test must be done on the default branch
  // because in the telemetry code, we're referencing the preference directly
  // instead of through NimbusFeatures. Enrolling in an experiment will change
  // the default branch, and not overwrite the user branch.
  prefBranch.setBoolPref(TELEMETRY_PREF, false);

  Assert.equal(
    lazy.serpEventsCategorizationEnabled,
    false,
    "serpEventsCategorizationEnabled should be false when not enrolled in experiment and the default value is false."
  );

  await lazy.ExperimentAPI.ready();

  info("Enroll in experiment.");
  let updateComplete = waitForDomainToCategoriesUpdate();

  let doExperimentCleanup = await lazy.ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.search.featureId,
      value: {
        serpEventTelemetryCategorizationEnabled: true,
      },
    },
    { isRollout: true }
  );

  Assert.equal(
    lazy.serpEventsCategorizationEnabled,
    true,
    "serpEventsCategorizationEnabled should be true when enrolled in experiment."
  );

  await updateComplete;

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a sample SERP with organic results.");
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
      locale: LOCALE,
      region: REGION,
      partner_code: "ff",
      provider: "example",
      tagged: "true",
      num_ads_clicked: "0",
      num_ads_visible: "2",
    },
  ]);
  resetTelemetry();

  info("End experiment.");
  await doExperimentCleanup();

  Assert.equal(
    lazy.serpEventsCategorizationEnabled,
    false,
    "serpEventsCategorizationEnabled should be false after experiment."
  );

  Assert.ok(
    lazy.SearchSERPDomainToCategoriesMap.empty,
    "Domain to categories map should be empty."
  );

  info("Load a sample SERP with organic results.");
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  // Wait an arbitrary amount for a possible categorization.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1500));
  BrowserTestUtils.removeTab(tab);

  assertCategorizationValues([]);

  // Clean up.
  prefBranch.setBoolPref(TELEMETRY_PREF, originalPrefValue);
});
