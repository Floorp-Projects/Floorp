/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests load SERPs in Private Browsing Mode. Existing tests do so in
 * non-Private Browsing Mode.
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [],
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
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.log", true],
      ["browser.search.serpEventTelemetry.enabled", true],
    ],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

add_task(async function load_2_pbm_serps_and_1_non_pbm_serp() {
  info("Open private browsing window.");
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  info("Load SERP in a new tab.");
  let impression = TestUtils.topicObserved("reported-page-with-impression");
  let url = getSERPUrl("searchTelemetry.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    privateWindow.gBrowser,
    url
  );
  info("Wait for page impression.");
  await impression;

  info("Load another SERP in the same tab.");
  impression = TestUtils.topicObserved("reported-page-with-impression");
  url = getSERPUrl("searchTelemetryAd.html");
  BrowserTestUtils.startLoadingURIString(privateWindow.gBrowser, url);
  info("Wait for page impression.");
  await impression;

  info("Close private window.");
  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(privateWindow);

  info("Load SERP in non-private window.");
  impression = TestUtils.topicObserved("reported-page-with-impression");
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  info("Wait for page impression.");
  await impression;

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "true",
        shopping_tab_displayed: "false",
      },
    },
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "true",
        shopping_tab_displayed: "false",
      },
    },
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

  // Clean-up.
  BrowserTestUtils.removeTab(tab);
});
