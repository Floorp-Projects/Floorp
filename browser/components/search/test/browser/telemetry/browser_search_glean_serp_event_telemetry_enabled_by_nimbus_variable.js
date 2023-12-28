/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test to verify we can toggle the Glean SERP event telemetry feature via a
// Nimbus variable.

const lazy = {};

const TELEMETRY_PREF = "browser.search.serpEventTelemetry.enabled";

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serpEventsEnabled",
  TELEMETRY_PREF,
  false
);

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

async function verifyEventsRecorded() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd.html")
  );
  await waitForPageWithAdImpressions();

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

  BrowserTestUtils.removeTab(tab);

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
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "2",
          ads_visible: "2",
          ads_hidden: "0",
        },
      ],
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.TAB_CLOSE,
      },
    },
  ]);
}

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
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
    lazy.serpEventsEnabled,
    false,
    "serpEventsEnabled should be false when not enrolled in experiment."
  );

  await lazy.ExperimentAPI.ready();

  let doExperimentCleanup = await lazy.ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.search.featureId,
      value: {
        serpEventTelemetryEnabled: true,
      },
    },
    { isRollout: true }
  );

  Assert.equal(
    lazy.serpEventsEnabled,
    true,
    "serpEventsEnabled should be true when enrolled in experiment."
  );

  // To ensure Nimbus set "browser.search.serpEventTelemetry.enabled" to true,
  // we test that an impression, ad_impression and abandonment event are
  // recorded correctly.
  await verifyEventsRecorded();

  await doExperimentCleanup();

  Assert.equal(
    lazy.serpEventsEnabled,
    false,
    "serpEventsEnabled should be false after experiment."
  );

  // Clean up.
  prefBranch.setBoolPref(TELEMETRY_PREF, originalPrefValue);
});
