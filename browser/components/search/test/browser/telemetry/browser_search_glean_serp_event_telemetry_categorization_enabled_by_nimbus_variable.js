/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test to verify we can toggle the Glean SERP event telemetry for SERP
// categorization feature via a Nimbus variable.

const lazy = {};
const TELEMETRY_PREF =
  "browser.search.serpEventTelemetryCategorization.enabled";

ChromeUtils.defineESModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  TELEMETRY_CATEGORIZATION_KEY:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
});

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  SearchSERPCategorization: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPDomainToCategoriesMap:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
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

const client = RemoteSettings(TELEMETRY_CATEGORIZATION_KEY);
const db = client.db;

let stub;
add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.log", true]],
  });

  // Clear existing Remote Settings data.
  await db.clear();
  info("Create record with attachment.");
  let { record, attachment } = await mockRecordWithAttachment({
    id: Services.uuid.generateUUID().number.slice(1, -1),
    version: 1,
    filename: "domain_category_mappings.json",
  });
  client.attachments.cacheImpl.set(record.id, attachment);
  await db.create(record);

  info("Add data to Remote Settings DB.");
  await db.importChanges({}, Date.now());

  stub = lazy.sinon.stub(lazy.SearchSERPCategorization, "dummyLogger");

  registerCleanupFunction(async () => {
    Services.telemetry.canRecordExtended = oldCanRecord;
    await SpecialPowers.popPrefEnv();
    resetTelemetry();
    await db.clear();
    client.attachments.cacheImpl.delete(record.id);
    stub.restore();
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
    "serpEventsCategorizationEnabled should be false when not enrolled in experiment."
  );

  await lazy.ExperimentAPI.ready();

  info("Enroll in experiment.");
  let updateComplete = TestUtils.topicObserved(
    "domain-to-categories-map-update-complete"
  );

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

  Assert.deepEqual(
    Array.from(stub.getCall(0).args[0]),
    ["foobar.org"],
    "Categorization of non-ads should match."
  );

  Assert.deepEqual(
    Array.from(stub.getCall(1).args[0]),
    ["abc.org", "def.org"],
    "Categorization of ads should match."
  );

  BrowserTestUtils.removeTab(tab);

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
  Assert.equal(
    stub.getCall(2),
    null,
    "dummyLogger should not have been called if experiment in un-enrolled."
  );

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  prefBranch.setBoolPref(TELEMETRY_PREF, originalPrefValue);
});
