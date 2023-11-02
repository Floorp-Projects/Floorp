/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for SearchSERPTelemetry associated with ad links found in data attributes.
 */

"use strict";

// Note: example.org is used for the SERP page, and example.com is used to serve
// the ads. This is done to simulate different domains like the real servers.
const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example-data-attributes",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_dataAttributes(?:_none|_href)?.html/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["xyz"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
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

add_task(async function test_track_ad_on_data_attributes() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd_dataAttributes.html")
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": {
        "example-data-attributes:tagged:ff": 1,
      },
      "browser.search.withads.unknown": {
        "example-data-attributes:tagged": 1,
      },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example-data-attributes",
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

add_task(async function test_track_ad_on_data_attributes_and_hrefs() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd_dataAttributes_href.html")
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": {
        "example-data-attributes:tagged:ff": 1,
      },
      "browser.search.withads.unknown": {
        "example-data-attributes:tagged": 1,
      },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example-data-attributes",
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

add_task(async function test_track_no_ad_on_data_attributes_and_hrefs() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd_dataAttributes_none.html")
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": {
        "example-data-attributes:tagged:ff": 1,
      },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example-data-attributes",
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
