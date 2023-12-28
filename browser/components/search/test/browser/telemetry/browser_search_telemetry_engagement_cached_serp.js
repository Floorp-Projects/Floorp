/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests check when a SERP retrieves data from the BFCache as SERPs
 * typically set their response headers with Cache-Control as private.
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_/,
    queryParamNames: ["s"],
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

async function goBack(tab, callback = async () => {}) {
  info("Go back.");
  let pageShowPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "pageshow"
  );
  tab.linkedBrowser.goBack();
  await pageShowPromise;
  await callback();
}

async function goForward(tab, callback = async () => {}) {
  info("Go forward.");
  let pageShowPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "pageshow"
  );
  tab.linkedBrowser.goForward();
  await pageShowPromise;
  await callback();
}

// This test loads a cached SERP and checks returning to it and interacting
// with elements on the page don't count the events more than once.
// This is a proxy for ensuring we remove event listeners.
add_task(async function test_cached_serp() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox.html");
  info("Load search page.");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  for (let index = 0; index < 3; ++index) {
    info("Load non-search page.");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser, true);
    BrowserTestUtils.startLoadingURIString(
      tab.linkedBrowser,
      "https://www.example.com"
    );
    await loadPromise;
    await goBack(tab, async () => {
      await waitForPageWithAdImpressions();
    });
  }

  info("Click on searchbox.");
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
    "There should be 1 engagement event recorded."
  );
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_back_and_forward_serp_to_serp() {
  await SpecialPowers.pushPrefEnv({
    // This has to be disabled or else using back and forward in the test won't
    // trigger responses in the network listener in SearchSERPTelemetry. The
    // page will still load from a BFCache.
    set: [["fission.bfcacheInParent", false]],
  });
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryAd_searchbox.html");
  info("Load search page.");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false);
  info("Click on a suggested search term.");
  BrowserTestUtils.synthesizeMouseAtCenter("#suggest", {}, tab.linkedBrowser);
  await loadPromise;
  await waitForPageWithAdImpressions();

  for (let index = 0; index < 3; ++index) {
    info("Return to first search page.");
    await goBack(tab, async () => {
      await waitForPageWithAdImpressions();
    });
    info("Return to second search page.");
    await goForward(tab, async () => {
      await waitForPageWithAdImpressions();
    });
  }

  await Services.fog.testFlushAllChildren();
  let engagements = Glean.serp.engagement.testGetValue() ?? [];
  let abandonments = Glean.serp.abandonment.testGetValue() ?? [];
  Assert.equal(engagements.length, 1, "There should be 1 engagement.");
  Assert.equal(abandonments.length, 6, "There should be 6 abandonments.");

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_back_and_forward_content_to_serp_to_serp() {
  await SpecialPowers.pushPrefEnv({
    // This has to be disabled or else using back and forward in the test won't
    // trigger responses in the network listener in SearchSERPTelemetry. The
    // page will still load from a BFCache.
    set: [["fission.bfcacheInParent", false]],
  });
  resetTelemetry();

  info("Load non-search page.");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://www.example.com/"
  );

  info("Load search page.");
  let url = getSERPUrl("searchTelemetryAd_searchbox.html");
  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser, true);
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
  await loadPromise;
  await waitForPageWithAdImpressions();

  loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false);
  info("Click on a suggested search term.");
  BrowserTestUtils.synthesizeMouseAtCenter("#suggest", {}, tab.linkedBrowser);
  await loadPromise;
  await waitForPageWithAdImpressions();

  info("Return to first search page.");
  await goBack(tab, async () => {
    await waitForPageWithAdImpressions();
  });

  info("Return to non-search page.");
  await goBack(tab);

  info("Return to first search page.");
  await goForward(tab, async () => {
    await waitForPageWithAdImpressions();
  });

  info("Return to second search page.");
  await goForward(tab, async () => {
    await waitForPageWithAdImpressions();
  });

  await Services.fog.testFlushAllChildren();
  let engagements = Glean.serp.engagement.testGetValue() ?? [];
  let abandonments = Glean.serp.abandonment.testGetValue() ?? [];
  Assert.equal(engagements.length, 1, "There should be 1 engagement.");
  Assert.equal(abandonments.length, 3, "There should be 3 abandonments.");

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
