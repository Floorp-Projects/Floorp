/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check SPA in-content interactions (e.g. search box, carousel arrow) and
 * ensures we're correctly unloading / adding listeners to elements.
 */

"use strict";

add_setup(async function () {
  await initSinglePageAppTest();

  await SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount.webIsolated", 1]],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

add_task(async function test_load_serp_and_click_searchbox() {
  resetTelemetry();

  let tab = await SinglePageAppUtils.createTabAndLoadURL();
  await SinglePageAppUtils.clickSearchbox(tab);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": {
        "example1:tagged:ff": 1,
      },
      "browser.search.withads.unknown": {
        "example1:tagged": 1,
      },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example1",
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
          target: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
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

  await BrowserTestUtils.removeTab(tab);
});

// This is to ensure if the user switches pages, we still keep track of the
// searchbox and don't add another listener.
add_task(async function test_load_serp_click_related_click_searchbox() {
  resetTelemetry();

  let tab = await SinglePageAppUtils.createTabAndLoadURL();
  await SinglePageAppUtils.visitRelatedSearch(tab);
  await SinglePageAppUtils.clickSearchbox(tab);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": {
        "example1:tagged:ff": 2,
      },
      "browser.search.withads.unknown": {
        "example1:tagged": 2,
      },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example1",
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
        provider: "example1",
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
          target: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
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

  await BrowserTestUtils.removeTab(tab);
});

// Click on another SERP tab and selecting the searchbox shouldn't cause a new
// engagement.
add_task(async function test_load_serp_click_related_click_searchbox() {
  resetTelemetry();

  let tab = await SinglePageAppUtils.createTabAndLoadURL();
  await SinglePageAppUtils.clickImagesTab(tab);
  await SinglePageAppUtils.clickSearchbox(tab);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": {
        "example1:tagged:ff": 1,
      },
      "browser.search.withads.unknown": {
        "example1:tagged": 1,
      },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example1",
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

  await BrowserTestUtils.removeTab(tab);
});

// Make sure unloading listeners is specific to the tab.
add_task(async function test_load_serp_click_related_click_searchbox() {
  resetTelemetry();

  let tab1 = await SinglePageAppUtils.createTabAndLoadURL();
  let tab2 = await SinglePageAppUtils.createTabAndLoadURL();

  // Listener should no longer be applicable on tab2 because we're switching
  // to tab2.
  await SinglePageAppUtils.clickImagesTab(tab2);
  await SinglePageAppUtils.clickSearchbox(tab2);

  // Click a searchbox on tab1 to verify the listener is still working.
  await SinglePageAppUtils.clickSearchbox(tab1);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": {
        "example1:tagged:ff": 2,
      },
      "browser.search.withads.unknown": {
        "example1:tagged": 2,
      },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example1",
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
          target: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
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
    {
      impression: {
        provider: "example1",
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

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
});
