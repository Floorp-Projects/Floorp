/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Checks that telemetry on the runtime performance of categorizing the SERP
 * works as normal.
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry(?:Ad)/,
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

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.serpEventTelemetry.enabled", true]],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

add_task(async function test_tab_contains_measurement() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd_components_text.html")
  );
  await waitForPageWithAdImpressions();

  await Services.fog.testFlushAllChildren();
  Assert.ok(
    Glean.serp.adImpression.testGetValue().length,
    "Should have received ad impressions."
  );

  let durations = Glean.serp.categorizationDuration.testGetValue();
  Assert.ok(durations.sum > 0, "Sum should be more than 0.");

  BrowserTestUtils.removeTab(tab);
});

// If the user opened a SERP and closed it quickly or navigated away from it
// and no ad impressions were recorded, we shouldn't record a measurement.
add_task(async function test_before_ad_impressions_recorded() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd_components_text.html")
  );
  BrowserTestUtils.removeTab(tab);

  Assert.ok(
    !Glean.serp.adImpression.testGetValue(),
    "Should not have an ad impression."
  );

  await Services.fog.testFlushAllChildren();
  let durations = Glean.serp.categorizationDuration.testGetValue();
  Assert.equal(durations, undefined, "Should not have received any values.");
});
