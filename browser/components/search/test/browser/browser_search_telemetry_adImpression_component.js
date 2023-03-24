/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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
    searchPageRegexp: /^http:\/\/mochi.test:.+\/browser\/browser\/components\/search\/test\/browser\/searchTelemetryAd_components_/,
    queryParamName: "s",
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad$/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_CAROUSEL,
        included: {
          parent: {
            selector: ".moz-carousel",
          },
          children: [
            {
              selector: ".moz-carousel-card",
              countChildren: true,
            },
          ],
        },
      },
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
      "http://mochi.test:8888"
    ) + page;
  return `${url}?s=test${organic ? "" : "&abc=ff"}`;
}

async function promiseAdImpressionReceived() {
  return TestUtils.waitForCondition(() => {
    let adImpressions = Glean.serp.adImpression.testGetValue() ?? [];
    return adImpressions.length;
  }, "Should have received an ad impression.");
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

add_task(async function test_ad_impressions_with_carousels() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_components_carousel.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await promiseAdImpressionReceived();

  assertAdImpressionEvents([
    {
      component: SearchSERPTelemetryUtils.COMPONENTS.AD_CAROUSEL,
      ads_loaded: "4",
      ads_visible: "4",
      ads_hidden: "0",
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_ad_impressions_with_carousels_tabhistory() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_components_carousel.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await promiseAdImpressionReceived();

  // Reset telemetry because we care about the telemetry upon going back.
  resetTelemetry();

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  BrowserTestUtils.loadURIString(
    tab.linkedBrowser,
    "https://www.example.com/some_url"
  );
  await browserLoadedPromise;

  let pageShowPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "pageshow"
  );
  tab.linkedBrowser.goBack();
  await pageShowPromise;

  await promiseAdImpressionReceived();

  assertAdImpressionEvents([
    {
      component: SearchSERPTelemetryUtils.COMPONENTS.AD_CAROUSEL,
      ads_loaded: "4",
      ads_visible: "4",
      ads_hidden: "0",
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_ad_impressions_with_hidden_carousels() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_components_carousel_hidden.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await promiseAdImpressionReceived();

  assertAdImpressionEvents([
    {
      component: SearchSERPTelemetryUtils.COMPONENTS.AD_CAROUSEL,
      ads_loaded: "3",
      ads_visible: "0",
      ads_hidden: "3",
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});
