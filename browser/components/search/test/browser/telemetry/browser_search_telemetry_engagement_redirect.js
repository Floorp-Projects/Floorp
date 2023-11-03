/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests load SERPs and click on both ad and non-ad links that can be
 * redirected.
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
    nonAdsLinkRegexps: [
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_nonAdsLink_redirect.html/,
    ],
    extraAdServersRegexps: [
      /^https:\/\/example\.com\/ad/,
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/redirect_ad/,
    ],
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

add_task(async function test_click_non_ads_link_redirected() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl = "https://example.com/hello_world";

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    targetUrl
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#non_ads_link_redirected",
    {},
    tab.linkedBrowser
  );

  await browserLoadedPromise;

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
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
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
  await waitForPageWithAdImpressions();

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
        is_private: "false",
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
  BrowserTestUtils.removeTab(tab2);
});

// Some providers load a URL of a non ad within a subframe before loading the
// target website in the top level frame.
add_task(async function test_click_non_ads_link_redirect_non_top_level() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl = "https://example.com/hello_world";

  let browserPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    targetUrl
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#non_ads_link_redirected_no_top_level",
    {},
    tab.linkedBrowser
  );

  await browserPromise;

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
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_multiple_redirects_non_ad_link() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl = "https://example.com/hello_world";

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    targetUrl
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#non_ads_link_multiple_redirects",
    {},
    tab.linkedBrowser
  );

  await browserLoadedPromise;

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
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_click_ad_link_redirected() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl = "https://example.com/hello_world";

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    targetUrl
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#ad_link_redirect",
    {},
    tab.linkedBrowser
  );

  await browserLoadedPromise;

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

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_click_ad_link_redirected_new_tab() {
  resetTelemetry();

  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl = "https://example.com/hello_world";

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, targetUrl, true);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#ad_link_redirect",
    { button: 1 },
    tab.linkedBrowser
  );
  let tab2 = await tabPromise;

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

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});
