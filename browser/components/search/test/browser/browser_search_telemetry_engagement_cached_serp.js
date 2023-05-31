/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test checks when a SERP is cached. Most of the time, SERPs aren't
 * cached, but it's possible for SERPs after the first page (e.g. page 2 of a
 * search term) to be cached.
 */

"use strict";

const { SearchSERPTelemetry, SearchSERPTelemetryUtils } =
  ChromeUtils.importESModule("resource:///modules/SearchSERPTelemetry.sys.mjs");

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/searchTelemetryAd_/,
    queryParamName: "s",
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [/^https:\/\/example.com/],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
        included: {
          parent: {
            selector: "form",
          },
          children: [
            {
              // This isn't contained in any of the HTML examples but the
              // presence of the entry ensures that if it is not found during
              // a topDown examination, the next element in the array is
              // inspected and found.
              selector: "textarea",
            },
            {
              selector: "input",
            },
          ],
          related: {
            selector: "div",
          },
        },
        topDown: true,
      },
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
];

function getSERPUrl(page, term) {
  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + page;
  return `${url}?s=${term}`;
}

async function waitForIdle() {
  for (let i = 0; i < 10; i++) {
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));
  }
}

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
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

// This test loads a cached SERP and checks returning to it and interacting
// with elements on the page don't count the events more than once.
// This is a proxy for ensuring we remove event listeners.
add_task(async function test_cached_serp() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  for (let index = 0; index < 3; ++index) {
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser, true);
    BrowserTestUtils.loadURIString(
      tab.linkedBrowser,
      "https://www.example.com"
    );
    await loadPromise;

    let pageShowPromise = BrowserTestUtils.waitForContentEvent(
      tab.linkedBrowser,
      "pageshow"
    );
    tab.linkedBrowser.goBack();
    await pageShowPromise;
    await waitForPageWithAdImpressions();
  }

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "input",
    {},
    tab.linkedBrowser
  );

  await Services.fog.testFlushAllChildren();
  let engagements = Glean.serp.engagement.testGetValue() ?? [];
  Assert.equal(
    engagements.length,
    1,
    "There should only be one engagement recorded."
  );
  BrowserTestUtils.removeTab(tab);
});
