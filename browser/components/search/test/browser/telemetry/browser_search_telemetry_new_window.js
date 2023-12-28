/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests load ads and organic links in new windows.
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
    set: [["browser.search.serpEventTelemetry.enabled", true]],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

add_task(async function load_serp_in_new_window_with_pref_and_click_ad() {
  info("Set browser.link.open_newwindow to open _blank in new window.");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 2]],
  });

  info("Load SERP in a new tab.");
  let url = getSERPUrl("searchTelemetryAd.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  info("Wait for page impression.");
  await waitForPageWithAdImpressions();

  info("Open ad link in a new window.");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.getElementById("ad1").setAttribute("target", "_blank");
  });
  let newWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: "https://example.com/ad",
  });
  await BrowserTestUtils.synthesizeMouseAtCenter("#ad1", {}, tab.linkedBrowser);
  let newWindow = await newWindowPromise;

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
      "browser.search.adclicks.unknown": { "example:tagged": 1 },
    }
  );

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
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "2",
          ads_visible: "2",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  // Clean-up.
  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(newWindow);
  resetTelemetry();
});

add_task(async function load_serp_in_new_window_with_pref_and_click_organic() {
  info("Set browser.link.open_newwindow to open _blank in new window.");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 2]],
  });

  info("Load SERP in a new tab.");
  let url = getSERPUrl("searchTelemetry.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  info("Wait for page impression.");
  await waitForPageWithAdImpressions();

  info("Open organic link in a new window.");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.querySelector("a").setAttribute("target", "_blank");
  });
  let newWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: "https://example.com/otherpage",
  });
  await BrowserTestUtils.synthesizeMouseAtCenter("a", {}, tab.linkedBrowser);
  let newWindow = await newWindowPromise;

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
    }
  );

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
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
      adImpressions: [],
    },
  ]);

  // Clean-up.
  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(newWindow);
  resetTelemetry();
});

add_task(async function load_serp_in_new_window_with_context_menu() {
  info("Load SERP in a new tab.");
  let url = getSERPUrl("searchTelemetryAd.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  info("Wait for page impression.");
  await waitForPageWithAdImpressions();

  info("Open context menu.");
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let contextMenuPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#ad1",
    { type: "contextmenu", button: 2 },
    tab.linkedBrowser
  );
  await contextMenuPromise;

  info("Click on Open Link in New Window");
  let newWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: "https://example.com/ad",
  });
  let openLinkInNewWindow = contextMenu.querySelector("#context-openlink");
  contextMenu.activateItem(openLinkInNewWindow);
  let newWindow = await newWindowPromise;

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
      "browser.search.adclicks.unknown": { "example:tagged": 1 },
    }
  );

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
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "2",
          ads_visible: "2",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  // Clean-up.
  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(newWindow);
  resetTelemetry();
});

add_task(
  async function load_multiple_serps_with_different_search_terms_and_click_ad() {
    info("Set browser.link.open_newwindow to open _blank in new window.");
    await SpecialPowers.pushPrefEnv({
      set: [["browser.link.open_newwindow", 2]],
    });

    info("Load SERP in a new tab.");
    let url = getSERPUrl("searchTelemetryAd.html");
    let formattedUrl1 = new URL(url);
    formattedUrl1.searchParams.set("s", "test1");
    let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    info("Wait for page impression.");
    await waitForPageWithAdImpressions();

    info("Load SERP in a new tab with a different search term.");
    url = getSERPUrl("searchTelemetryAd.html");
    let formattedUrl2 = new URL(url);
    formattedUrl2.searchParams.set("s", "test2");
    let tab2 = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      formattedUrl2.href
    );
    info("Wait for page impression of tab 2.");
    await waitForPageWithAdImpressions();

    Assert.notEqual(
      formattedUrl1.searchParams.get("s"),
      formattedUrl2.searchParams.get("s"),
      "The search query param in both tabs are different."
    );

    info("Open ad link of tab 2 in a new window.");
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
      content.document.getElementById("ad1").setAttribute("target", "_blank");
    });
    let newWindowPromise = BrowserTestUtils.waitForNewWindow({
      url: "https://example.com/ad",
    });
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#ad1",
      {},
      tab2.linkedBrowser
    );
    let newWindow = await newWindowPromise;

    await assertSearchSourcesTelemetry(
      {},
      {
        "browser.search.content.unknown": { "example:tagged:ff": 2 },
        "browser.search.withads.unknown": { "example:tagged": 2 },
        "browser.search.adclicks.unknown": { "example:tagged": 1 },
      }
    );

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
        adImpressions: [
          {
            component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
            ads_loaded: "2",
            ads_visible: "2",
            ads_hidden: "0",
          },
        ],
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
        engagements: [
          {
            action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
            target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          },
        ],
        adImpressions: [
          {
            component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
            ads_loaded: "2",
            ads_visible: "2",
            ads_hidden: "0",
          },
        ],
      },
    ]);

    // Clean-up.
    await SpecialPowers.popPrefEnv();
    BrowserTestUtils.removeTab(tab1);
    BrowserTestUtils.removeTab(tab2);
    await BrowserTestUtils.closeWindow(newWindow);
    resetTelemetry();
  }
);
