/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests load SERPs and click on links that can either be ads or non ads
 * and verifies that the engagement events and the target associated with them
 * are correct.
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
    searchPageRegexp: /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/searchTelemetryAd_/,
    queryParamName: "s",
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/searchTelemetryAd_nonAdsLink_redirect/,
    ],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
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
          related: {
            selector: "button",
          },
        },
      },
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        included: {
          parent: {
            selector: ".moz_ad",
          },
          children: [
            {
              selector: ".multi-col",
              type: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
            },
          ],
          related: {
            selector: "button",
          },
        },
        excluded: {
          parent: {
            selector: ".rhs",
          },
        },
      },
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
        nonAd: true,
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

// This is used to check if not providing an nonAdsLinkRegexp can still
// reliably categorize non_ads_links.
const TEST_PROVIDER_INFO_NO_NON_ADS_REGEXP = [
  {
    ...TEST_PROVIDER_INFO[0],
    nonAdsLinkRegexps: [],
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

async function promiseImpressionReceived() {
  return TestUtils.waitForCondition(() => {
    let adImpressions = Glean.serp.adImpression.testGetValue() ?? [];
    return adImpressions.length;
  }, "Should have received an ad impression.");
}

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

// This test ensures clicking a non-first link in a component registers the
// proper component. This is because the first link of a component does the
// heavy lifting in finding the parent and best categorization of the
// component. Subsequent anchors that have the same parent get grouped into it.
// Additionally, this test deliberately has ads with different paths so that
// there are no collisions in hrefs.
add_task(async function test_click_second_ad_in_component() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await promiseImpressionReceived();

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.getElementById("deep_ad_sitelink").click();
  });
  await pageLoadPromise;

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

// If a provider does a re-direct and we open it in a new tab, we should
// record the click and have the correct number of engagements.
add_task(async function test_click_non_ads_link_redirected_new_tab() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promiseImpressionReceived();

  let redirectUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_nonAdsLink_redirect.html";
  let targetUrl = "https://example.com/hello_world";

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, targetUrl, true);

  await SpecialPowers.spawn(tab.linkedBrowser, [redirectUrl], urls => {
    content.document
      .getElementById(["non_ads_link"])
      .addEventListener("click", e => {
        e.preventDefault();
        content.window.open([urls], "_blank");
      });
    content.document.getElementById("non_ads_link").click();
  });
  let tab2 = await tabPromise;

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab);
});

// If a provider does a re-direct in the same tab, we may not be able to
// determine the component based on the URLs on the page, because the initial
// redirect URL won't be known, and by the time the page is loaded, the cached
// data might be invalidated. So this should use regular expressions to make
// an inference.
add_task(async function test_click_non_ads_link_redirected() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promiseImpressionReceived();

  let redirectUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_nonAdsLink_redirect.html";
  let targetUrl = "https://example.com/hello_world";

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    targetUrl
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [redirectUrl], urls => {
    content.document
      .getElementById(["non_ads_link"])
      .setAttribute("href", [urls]);
    content.document.getElementById("non_ads_link").click();
  });
  await browserLoadedPromise;

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

// If a provider appends query parameters to a link after the page has been
// parsed, we should still be able to record the click.
add_task(async function test_click_ads_link_modified() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promiseImpressionReceived();

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    let target = content.document.getElementById("deep_ad_sitelink");
    let href = target.getAttribute("href");
    target.setAttribute("href", href + "?foo=bar");
    content.document.getElementById("deep_ad_sitelink").click();
  });
  await browserLoadedPromise;

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

// If an anchor is a non_ads_link and we don't have a matching regular
// expression, we should still be categorize it as non ads.
add_task(async function test_click_non_ads_link() {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(
    TEST_PROVIDER_INFO_NO_NON_ADS_REGEXP
  );
  await waitForIdle();

  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await promiseImpressionReceived();

  // Click a non ad.
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.getElementById("non_ads_link").click();
  });
  await pageLoadPromise;

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

// Search box is a special case which has to be tracked in the child process.
add_task(async function test_click_incontent_searchbox() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await promiseImpressionReceived();

  // Click on the searchbox.
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    let input = content.document.querySelector("form input");
    input.click();
    input.focus();
  });
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  EventUtils.synthesizeKey("KEY_Enter");
  await pageLoadPromise;

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
        },
        {
          action: SearchSERPTelemetryUtils.ACTIONS.SUBMITTED,
          target: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

// Click an auto-suggested term. The element that is clicked is related
// to the searchbox but not in search-telemetry-v2 because it can be too
// difficult to determine ahead of time since the elements are generated
// dynamically. So instead it should listen to an element higher in the DOM.
add_task(async function test_click_autosuggest() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await promiseImpressionReceived();

  // Click an autosuggested term.
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.querySelector("#suggest").click();
  });
  await pageLoadPromise;

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.SUBMITTED,
          target: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

// Carousel related buttons expand content.
add_task(async function test_click_carousel_expand() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_components_carousel.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await promiseImpressionReceived();

  // Click a button that is expected to expand.
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.querySelector("button").click();
  });

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.EXPANDED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_CAROUSEL,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});
