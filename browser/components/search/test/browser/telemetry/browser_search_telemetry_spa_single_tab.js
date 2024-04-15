/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests check on SPA page loads in a single tab.
 * They also ensure the SinglePageAppUtils method work as expected.
 */

"use strict";

add_setup(async function () {
  await initSinglePageAppTest();

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

add_task(async function test_load_serp() {
  resetTelemetry();
  let tab = await SinglePageAppUtils.createTabAndLoadURL();
  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example1:tagged": 1 },
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
        is_signed_in: "false",
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
  ]);
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_load_serp_and_push_unrelated_state() {
  resetTelemetry();
  let tab = await SinglePageAppUtils.createTabAndLoadURL();
  let searchParams = new URL(tab.linkedBrowser.currentURI.spec).searchParams;

  Assert.equal(
    searchParams.get("foobar"),
    null,
    "Query param value for: foobar"
  );

  await SinglePageAppUtils.pushUnrelatedState(tab, {
    key: "foobar",
    value: "baz",
  });
  searchParams = new URL(tab.linkedBrowser.currentURI.spec).searchParams;
  Assert.equal(
    searchParams.get("foobar"),
    "baz",
    "Query param value for: foobar"
  );

  // If the SERP adds query parameter unrelated to the search query or the
  // query param matching the default results page, we shouldn't record another
  // SERP load.
  /* eslint-disable-next-line mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 1000));
  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example1:tagged": 1 },
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
        is_signed_in: "false",
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
  ]);
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_load_serp_and_load_non_serp_tab() {
  resetTelemetry();
  let tab = await SinglePageAppUtils.createTabAndLoadURL();

  await SinglePageAppUtils.clickImagesTab(tab);
  // If clicking another tab in a SPA, we shouldn't record another SERP load.
  /* eslint-disable-next-line mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 1000));
  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example1:tagged": 1 },
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
        is_signed_in: "false",
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

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_load_serp_and_click_ad() {
  resetTelemetry();
  let tab = await SinglePageAppUtils.createTabAndLoadURL();

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example1:tagged": 1 },
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
        is_signed_in: "false",
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
  ]);

  await SinglePageAppUtils.clickAd(tab);
  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example1:tagged": 1 },
      "browser.search.adclicks.unknown": { "example1:tagged": 1 },
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
        is_signed_in: "false",
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

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_load_serp_and_click_redirect_ad() {
  resetTelemetry();
  let tab = await SinglePageAppUtils.createTabAndLoadURL();

  await SinglePageAppUtils.clickRedirectAd(tab);
  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example1:tagged": 1 },
      "browser.search.adclicks.unknown": { "example1:tagged": 1 },
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
        is_signed_in: "false",
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
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_load_serp_and_click_redirect_ad_in_new_tab() {
  resetTelemetry();
  let tab = await SinglePageAppUtils.createTabAndLoadURL();

  let redirectedTab = await SinglePageAppUtils.clickRedirectAdInNewTab(tab);
  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example1:tagged": 1 },
      "browser.search.adclicks.unknown": { "example1:tagged": 1 },
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
        is_signed_in: "false",
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

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(redirectedTab);
});

add_task(async function test_load_serp_click_a_related_search() {
  resetTelemetry();
  let tab = await SinglePageAppUtils.createTabAndLoadURL();
  await SinglePageAppUtils.visitRelatedSearch(tab);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 2 },
      "browser.search.withads.unknown": { "example1:tagged": 2 },
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
        is_signed_in: "false",
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
        is_signed_in: "false",
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
  ]);

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_load_serp_click_a_related_search_click_ad() {
  resetTelemetry();
  let tab = await SinglePageAppUtils.createTabAndLoadURL();
  await SinglePageAppUtils.visitRelatedSearch(tab);
  await SinglePageAppUtils.clickAd(tab);
  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 2 },
      "browser.search.withads.unknown": { "example1:tagged": 2 },
      "browser.search.adclicks.unknown": { "example1:tagged": 1 },
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
        is_signed_in: "false",
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
        is_signed_in: "false",
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

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_load_serp_click_non_serp_tab_click_all() {
  resetTelemetry();
  let tab = await SinglePageAppUtils.createTabAndLoadURL();
  await SinglePageAppUtils.clickImagesTab(tab);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example1:tagged": 1 },
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
        is_signed_in: "false",
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

  info("Click All tab to return to a SERP.");
  await SinglePageAppUtils.clickAllTab(tab);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 2 },
      "browser.search.withads.unknown": { "example1:tagged": 2 },
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
        is_signed_in: "false",
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
        is_signed_in: "false",
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
  ]);

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_load_serp_and_use_back_and_forward() {
  resetTelemetry();
  let tab = await SinglePageAppUtils.createTabAndLoadURL();
  await SinglePageAppUtils.visitRelatedSearch(tab);
  await SinglePageAppUtils.goBack(tab);
  await SinglePageAppUtils.goForward(tab);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example1:tagged:ff": 2 },
      "browser.search.withads.unknown": { "example1:tagged": 2 },
      "browser.search.content.tabhistory": { "example1:tagged:ff": 2 },
      "browser.search.withads.tabhistory": { "example1:tagged": 2 },
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
        is_signed_in: "false",
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
        is_signed_in: "false",
      },
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "2",
          ads_visible: "2",
          ads_hidden: "0",
        },
      ],
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
      },
    },
    {
      impression: {
        provider: "example1",
        tagged: "true",
        partner_code: "ff",
        source: "tabhistory",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
        is_signed_in: "false",
      },
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "2",
          ads_visible: "2",
          ads_hidden: "0",
        },
      ],
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
      },
    },
    {
      impression: {
        provider: "example1",
        tagged: "true",
        partner_code: "ff",
        source: "tabhistory",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
        is_signed_in: "false",
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
  ]);

  await BrowserTestUtils.removeTab(tab);
});
