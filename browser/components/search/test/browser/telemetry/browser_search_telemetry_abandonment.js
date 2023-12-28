/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for the Glean SERP abandonment event
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry(?:Ad)?/,
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
    set: [["browser.search.serpEventTelemetry.enabled", true]],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
  });
});

add_task(async function test_tab_close() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetry.html")
  );

  await waitForPageWithAdImpressions();

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
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.TAB_CLOSE,
      },
    },
  ]);
});

add_task(async function test_window_close() {
  resetTelemetry();

  let serpUrl = getSERPUrl("searchTelemetry.html");
  let otherWindow = await BrowserTestUtils.openNewBrowserWindow();
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    otherWindow.gBrowser,
    false,
    serpUrl
  );
  BrowserTestUtils.startLoadingURIString(otherWindow.gBrowser, serpUrl);
  await browserLoadedPromise;
  await waitForPageWithAdImpressions();

  await BrowserTestUtils.closeWindow(otherWindow);

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
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.WINDOW_CLOSE,
      },
    },
  ]);
});

add_task(async function test_navigation_via_urlbar() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetry.html")
  );
  await waitForPageWithAdImpressions();

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    gBrowser,
    false,
    "https://www.example.com/"
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, "https://www.example.com");
  await browserLoadedPromise;

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
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_navigation_via_back_button() {
  resetTelemetry();

  let exampleUrl = "https://example.com/";
  let serpUrl = getSERPUrl("searchTelemetry.html");
  await BrowserTestUtils.withNewTab(exampleUrl, async browser => {
    info("example.com is now loaded.");

    let pageLoadPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      serpUrl
    );
    BrowserTestUtils.startLoadingURIString(browser, serpUrl);
    await pageLoadPromise;
    info("Serp is now loaded.");

    let pageShowPromise = BrowserTestUtils.waitForContentEvent(
      browser,
      "pageshow"
    );
    browser.goBack();
    await pageShowPromise;

    info("Previous page (example.com) is now loaded after back navigation.");
  });

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
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
      },
    },
  ]);
});

add_task(async function test_click_ad() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd.html")
  );

  await TestUtils.waitForCondition(() => {
    let adImpressions = Glean.serp.adImpression.testGetValue() ?? [];
    return adImpressions.length;
  }, "Should have received an ad impression.");

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a",
    {},
    gBrowser.selectedBrowser
  );
  await browserLoadedPromise;

  Assert.equal(
    !!Glean.serp.abandonment.testGetValue(),
    false,
    "Should not have any abandonment events."
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_click_non_ad() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd_components_text.html")
  );
  await waitForPageWithAdImpressions();

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#non_ads_link",
    {},
    tab.linkedBrowser
  );
  await pageLoadPromise;

  Assert.equal(
    !!Glean.serp.abandonment.testGetValue(),
    false,
    "Should not have any abandonment events."
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_without_components() {
  // Mock a provider that doesn't have components.
  let providerInfo = [
    {
      ...TEST_PROVIDER_INFO[0],
      components: [],
    },
  ];
  SearchSERPTelemetry.overrideSearchTelemetryForTests(providerInfo);
  await waitForIdle();
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd.html")
  );

  // We shouldn't expect a SERP impression, so instead wait roughly
  // around how long it would usually take to receive an impression following
  // a page load.
  await promiseWaitForAdLinkCheck();
  Assert.equal(
    !!Glean.serp.impression.testGetValue(),
    false,
    "Should not have any impression events."
  );

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    gBrowser,
    false,
    "https://www.example.com/"
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, "https://www.example.com");
  await browserLoadedPromise;

  Assert.equal(
    !!Glean.serp.abandonment.testGetValue(),
    false,
    "Should not have any abandonment events."
  );

  BrowserTestUtils.removeTab(tab);

  // Allow subsequent tests to use the default provider.
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
});
