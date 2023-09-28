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
  SearchSERPCategorization: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPDomainToCategoriesMap:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serpEventsCategorizationEnabled",
  TELEMETRY_PREF,
  false
);

function testCategorizationLogic() {
  const TEST_DOMAIN_TO_CATEGORIES_MAP = {
    "byVQ4ej7T7s2xf/cPqgMyw==": [2, 90],
    "1TEnSjgNCuobI6olZinMiQ==": [2, 95],
    "/Bnju09b9iBPjg7K+5ENIw==": [2, 78, 4, 10],
    "Ja6RJq5LQftdl7NQrX1avQ==": [2, 56, 4, 24],
    "Jy26Qt99JrUderAcURtQ5A==": [2, 89],
    "sZnJyyzY9QcN810Q6jfbvw==": [2, 43],
    "QhmteGKeYk0okuB/bXzwRw==": [2, 65],
    "CKQZZ1IJjzjjE4LUV8vUSg==": [2, 67],
    "FK7mL5E1JaE6VzOiGMmlZg==": [2, 89],
    "mzcR/nhDcrs0ed4kTf+ZFg==": [2, 99],
  };

  lazy.SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP
  );

  let domains = new Set([
    "test1.com",
    "test2.com",
    "test3.com",
    "test4.com",
    "test5.com",
    "test6.com",
    "test7.com",
    "test8.com",
    "test9.com",
    "test10.com",
  ]);

  let resultsToReport =
    lazy.SearchSERPCategorization.applyCategorizationLogic(domains);

  Assert.deepEqual(
    resultsToReport,
    { category: "2", num_domains: 10, num_inconclusive: 0, num_unknown: 0 },
    "Should report the correct values for categorizing the SERP."
  );
}

add_setup(async function () {
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.log", true]],
  });

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
    "serpEventsCategorizationEnabled should be false when not enrolled in experiment."
  );

  await lazy.ExperimentAPI.ready();

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

  // To ensure Nimbus set
  // "browser.search.serpEventCategorizationTelemetry.enabled" to
  // true, we test that we're able to categorize a set of domains.
  testCategorizationLogic();

  await doExperimentCleanup();

  Assert.equal(
    lazy.serpEventsCategorizationEnabled,
    false,
    "serpEventsCategorizationEnabled should be false after experiment."
  );

  // Clean up.
  prefBranch.setBoolPref(TELEMETRY_PREF, originalPrefValue);
});
