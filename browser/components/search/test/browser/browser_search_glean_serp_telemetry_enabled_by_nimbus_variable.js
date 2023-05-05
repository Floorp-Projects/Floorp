/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test to verify we can toggle the Glean SERP telemetry feature via a Nimbus
// variable.

const {
  SearchSERPTelemetry,
  SearchSERPTelemetryUtils,
} = ChromeUtils.importESModule(
  "resource:///modules/SearchSERPTelemetry.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serpEventsEnabled",
  "browser.search.serpEventTelemetry.enabled",
  false
);

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp: /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/searchTelemetry(?:Ad)?.html/,
    queryParamName: "s",
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
  function getSERPUrl(page, organic = false) {
    let url =
      getRootDirectory(gTestPath).replace(
        "chrome://mochitests/content",
        "https://example.org"
      ) + page;
    return `${url}?s=test${organic ? "" : "&abc=ff"}`;
  }

  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd.html")
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  await waitForPageWithAdImpressions();

  assertAdImpressionEvents([
    {
      component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
      ads_loaded: "2",
      ads_visible: "2",
      ads_hidden: "0",
    },
  ]);

  BrowserTestUtils.removeTab(tab);

  assertAbandonmentEvent({
    abandonment: {
      reason: SearchSERPTelemetryUtils.ABANDONMENTS.TAB_CLOSE,
    },
  });
}

// sharedData messages are only passed to the child on idle. Therefore
// we wait for a few idles to try and ensure the messages have been able
// to be passed across and handled.
async function waitForIdle() {
  for (let i = 0; i < 10; i++) {
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));
  }
}

add_setup(async function() {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.log", true]],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    await SpecialPowers.popPrefEnv();
    resetTelemetry();
  });
});

add_task(async function test_enable_experiment() {
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
});
