/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for SearchSERPTelemetry associated with ad clicks.
 */

"use strict";

// Note: example.org is used for the SERP page, and example.com is used to serve
// the ads. This is done to simulate different domains like the real servers.
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

async function track_ad_click(testOrganic) {
  // Note: the above tests have already checked a page with no ad-urls.
  resetTelemetry();

  let expectedScalarKey = `example:${testOrganic ? "organic" : "tagged"}`;
  let expectedContentScalarKey = `example:${
    testOrganic ? "organic:none" : "tagged:ff"
  }`;
  let tagged = testOrganic ? "false" : "true";
  let partnerCode = testOrganic ? "" : "ff";

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd.html", testOrganic)
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { [expectedContentScalarKey]: 1 },
      "browser.search.withads.unknown": {
        [expectedScalarKey.replace("sap", "tagged")]: 1,
      },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);
  await promiseAdImpressionReceived(1);

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#ad1", {}, tab.linkedBrowser);
  await pageLoadPromise;
  await promiseWaitForAdLinkCheck();

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { [expectedContentScalarKey]: 1 },
      "browser.search.withads.unknown": { [expectedScalarKey]: 1 },
      "browser.search.adclicks.unknown": { [expectedScalarKey]: 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
    },
  ]);

  // Now go back, and click again.
  pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  gBrowser.goBack();
  await pageLoadPromise;
  await promiseWaitForAdLinkCheck();

  // We've gone back, so we register an extra display & if it is with ads or not.
  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.tabhistory": { [expectedContentScalarKey]: 1 },
      "browser.search.content.unknown": { [expectedContentScalarKey]: 1 },
      "browser.search.withads.tabhistory": { [expectedScalarKey]: 1 },
      "browser.search.withads.unknown": { [expectedScalarKey]: 1 },
      "browser.search.adclicks.unknown": { [expectedScalarKey]: 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "tabhistory",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);
  await promiseAdImpressionReceived(2);

  pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#ad1", {}, tab.linkedBrowser);
  await pageLoadPromise;
  await promiseWaitForAdLinkCheck();

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.tabhistory": { [expectedContentScalarKey]: 1 },
      "browser.search.content.unknown": { [expectedContentScalarKey]: 1 },
      "browser.search.withads.tabhistory": { [expectedScalarKey]: 1 },
      "browser.search.withads.unknown": { [expectedScalarKey]: 1 },
      "browser.search.adclicks.tabhistory": { [expectedScalarKey]: 1 },
      "browser.search.adclicks.unknown": { [expectedScalarKey]: 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "tabhistory",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_track_ad_click() {
  await track_ad_click(false);
});

add_task(async function test_track_ad_click_organic() {
  await track_ad_click(true);
});

add_task(async function test_track_ad_click_with_location_change_other_tab() {
  resetTelemetry();
  const url = getSERPUrl("searchTelemetryAd.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
    }
  );

  assertImpressionEvents([
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
    },
  ]);
  await promiseAdImpressionReceived();

  const newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );

  await BrowserTestUtils.switchTab(gBrowser, tab);

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#ad1", {}, tab.linkedBrowser);
  await pageLoadPromise;

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
      "browser.search.adclicks.unknown": { "example:tagged": 1 },
    }
  );

  assertImpressionEvents([
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
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(tab);
});
