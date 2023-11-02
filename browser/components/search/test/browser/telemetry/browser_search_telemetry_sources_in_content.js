/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * SearchSERPTelemetry tests related to in-content sources.
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
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        included: {
          parent: {
            selector: ".refined-search-buttons",
          },
          children: [
            {
              selector: "a",
            },
          ],
        },
        topDown: true,
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
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  Services.prefs.setBoolPref("browser.search.log", true);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.search.log");
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
  });
});

add_task(async function test_source_opened_in_new_tab_via_middle_click() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_searchbox_with_content.html?s=test+one+two+three";

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, targetUrl, true);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a#related-in-page",
    { button: 1 },
    tab1.linkedBrowser
  );
  let tab2 = await tabPromise;

  await TestUtils.waitForCondition(() => {
    return Glean.serp.impression?.testGetValue()?.length == 2;
  }, "Should have two impressions.");

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
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
        source: "opened_in_new_tab",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

add_task(async function test_source_opened_in_new_tab_via_target_blank() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_searchbox_with_content.html?s=test+one+two+three";

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, targetUrl, true);
  // Note: the anchor element with id "related-new-tab" has a target=_blank
  // attribute.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a#related-new-tab",
    {},
    tab1.linkedBrowser
  );
  let tab2 = await tabPromise;

  await TestUtils.waitForCondition(() => {
    return Glean.serp.impression?.testGetValue()?.length == 2;
  }, "Should have two impressions.");

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
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
        source: "opened_in_new_tab",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

add_task(async function test_source_opened_in_new_tab_via_context_menu() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_searchbox_with_content.html?s=test+one+two+three";

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, targetUrl, true);

  let contextMenu = document.getElementById("contentAreaContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a#related-in-page",
    {
      button: 2,
      type: "contextmenu",
    },
    tab1.linkedBrowser
  );
  await popupShownPromise;

  let openLinkInNewTabMenuItem = contextMenu.querySelector(
    "#context-openlinkintab"
  );
  contextMenu.activateItem(openLinkInNewTabMenuItem);

  let tab2 = await tabPromise;

  await TestUtils.waitForCondition(() => {
    return Glean.serp.impression?.testGetValue()?.length == 2;
  }, "Should have two impressions.");

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
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
        source: "opened_in_new_tab",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

add_task(
  async function test_source_refinement_button_clicked_no_partner_code() {
    resetTelemetry();
    let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    await waitForPageWithAdImpressions();

    let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#refined-search-button",
      {},
      tab.linkedBrowser
    );
    await pageLoadPromise;

    await TestUtils.waitForCondition(() => {
      return Glean.serp.impression?.testGetValue()?.length == 2;
    }, "Should have two impressions.");

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
            target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
          },
        ],
      },
      {
        impression: {
          provider: "example",
          tagged: "false",
          partner_code: "",
          source: "follow_on_from_refine_on_SERP",
          is_shopping_page: "false",
          is_private: "false",
          shopping_tab_displayed: "false",
        },
      },
    ]);

    BrowserTestUtils.removeTab(tab);
  }
);

add_task(
  async function test_source_refinement_button_clicked_with_partner_code() {
    resetTelemetry();
    let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    await waitForPageWithAdImpressions();

    let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#refined-search-button-with-partner-code",
      {},
      tab.linkedBrowser
    );
    await pageLoadPromise;

    await TestUtils.waitForCondition(() => {
      return Glean.serp.impression?.testGetValue()?.length == 2;
    }, "Should have two impressions.");

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
            target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
          },
        ],
      },
      {
        impression: {
          provider: "example",
          tagged: "true",
          partner_code: "ff",
          source: "follow_on_from_refine_on_SERP",
          is_shopping_page: "false",
          is_private: "false",
          shopping_tab_displayed: "false",
        },
      },
    ]);

    BrowserTestUtils.removeTab(tab);
  }
);

// When a user opens a refinement button link in a new tab, we want the
// source to be recorded as "follow_on_from_refine_on_SERP", not
// "opened_in_new_tab", since the refinement button click provides greater
// context.
add_task(async function test_refinement_button_vs_opened_in_new_tab() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let targetUrl =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_searchbox_with_content.html?s=test2&abc=ff";

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, targetUrl, true);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#refined-search-button-with-partner-code",
    { button: 1 },
    tab1.linkedBrowser
  );
  let tab2 = await tabPromise;

  await TestUtils.waitForCondition(() => {
    return Glean.serp.impression?.testGetValue()?.length == 2;
  }, "Should have two impressions.");

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
          target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "follow_on_from_refine_on_SERP",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
