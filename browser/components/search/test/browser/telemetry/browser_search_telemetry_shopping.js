/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check the existence of a shopping tab and navigation to a shopping page.
 * Most existing tests don't include shopping tabs, so this explicitly loads a
 * page with a shopping tab and clicks on it.
 */

"use strict";

// The setup for each test is the same, the only differences are the various
// permutations of the search tests.
const BASE_TEST_PROVIDER = {
  telemetryId: "example",
  searchPageRegexp:
    /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd/,
  queryParamNames: ["s"],
  codeParamName: "abc",
  taggedCodes: ["ff"],
  extraAdServersRegexps: [/^https:\/\/example\.org\/ad/],
  components: [
    {
      type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
      default: true,
    },
  ],
};

const TEST_PROVIDER_INFO_1 = [
  {
    ...BASE_TEST_PROVIDER,
    shoppingTab: {
      selector: "nav a",
      regexp: "&page=shopping&",
      inspectRegexpInSERP: true,
    },
  },
];

const TEST_PROVIDER_INFO_2 = [
  {
    ...BASE_TEST_PROVIDER,
    shoppingTab: {
      selector: "nav a#shopping",
      regexp: "&page=shopping&",
      inspectRegexpInSERP: false,
    },
  },
];

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO_1);
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

async function loadSerpAndClickShoppingTab(page) {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl(page)
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
        shopping_tab_displayed: "true",
      },
    },
  ]);
  await waitForPageWithAdImpressions();

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#shopping", {}, tab.linkedBrowser);
  await pageLoadPromise;

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "true",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.SHOPPING_TAB,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_inspect_shopping_tab_regexp_on_serp() {
  resetTelemetry();
  await loadSerpAndClickShoppingTab("searchTelemetryAd_shopping.html");
});

add_task(async function test_no_inspect_shopping_tab_regexp_on_serp() {
  resetTelemetry();
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO_2);
  await waitForIdle();
  await loadSerpAndClickShoppingTab("searchTelemetryAd_shopping.html");
});
