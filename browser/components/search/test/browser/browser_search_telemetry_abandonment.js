/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for the Glean SERP abandonment event
 */

"use strict";

const {
  SearchSERPTelemetry,
  SearchSERPTelemetryUtils,
} = ChromeUtils.importESModule(
  "resource:///modules/SearchSERPTelemetry.sys.mjs"
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
        included: {
          default: true,
        },
      },
    ],
  },
];

function getSERPUrl(page, organic = false) {
  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + page;
  return `${url}?s=test${organic ? "" : "&abc=ff"}`;
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

add_task(async function test_tab_close() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetry.html")
  );

  BrowserTestUtils.removeTab(tab);

  assertAbandonmentEvent({
    abandonment: {
      reason: SearchSERPTelemetryUtils.ABANDONMENTS.TAB_CLOSE,
    },
  });
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
  BrowserTestUtils.loadURIString(otherWindow.gBrowser, serpUrl);
  await browserLoadedPromise;

  await BrowserTestUtils.closeWindow(otherWindow);

  assertAbandonmentEvent({
    abandonment: {
      reason: SearchSERPTelemetryUtils.ABANDONMENTS.WINDOW_CLOSE,
    },
  });
});

add_task(async function test_navigation_via_urlbar() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetry.html")
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    gBrowser,
    false,
    "https://www.example.com/"
  );
  BrowserTestUtils.loadURIString(gBrowser, "https://www.example.com");
  await browserLoadedPromise;

  assertAbandonmentEvent({
    abandonment: {
      reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
    },
  });

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
    BrowserTestUtils.loadURIString(browser, serpUrl);
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

  assertAbandonmentEvent({
    abandonment: {
      reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
    },
  });
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
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.querySelector("a").click();
  });
  await browserLoadedPromise;

  Assert.equal(
    !!Glean.serp.abandonment.testGetValue(),
    false,
    "Should not have any abandonment events."
  );

  BrowserTestUtils.removeTab(tab);
});
