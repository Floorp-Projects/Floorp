/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for SearchSERPTelemetry associated with ad links and load events.
 */

"use strict";

// Note: example.org is used for the SERP page, and example.com is used to serve
// the ads. This is done to simulate different domains like the real servers.
const TEST_PROVIDER_INFO = [
  {
    telemetryId: "slow-page-load",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/slow_loading_page_with_ads(_on_load_event)?.html/,
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
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.log", true],
      ["browser.search.serpEventTelemetry.enabled", true],
    ],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
  });
});

add_task(async function test_track_ad_on_DOMContentLoaded() {
  resetTelemetry();

  let observeAdPreviouslyRecorded = TestUtils.consoleMessageObserved(msg => {
    return (
      typeof msg.wrappedJSObject.arguments?.[0] == "string" &&
      msg.wrappedJSObject.arguments[0].includes(
        "Ad was previously reported for browser with URI"
      )
    );
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("slow_loading_page_with_ads.html")
  );

  // Observe ad was counted on DOMContentLoaded.
  // We do not count the ad again on load.
  await observeAdPreviouslyRecorded;

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "slow-page-load:tagged:ff": 1 },
      "browser.search.withads.unknown": { "slow-page-load:tagged": 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "slow-page-load",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_ad_on_load_event() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("slow_loading_page_with_ads_on_load_event.html")
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "slow-page-load:tagged:ff": 1 },
      "browser.search.withads.unknown": { "slow-page-load:tagged": 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "slow-page-load",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});
