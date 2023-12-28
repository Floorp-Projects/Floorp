/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Main tests for SearchSERPTelemetry - general engine visiting and link clicking.
 */

"use strict";

// Note: example.org is used for the SERP page, and example.com is used to serve
// the ads. This is done to simulate different domains like the real servers.
const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry(?:Ad)?.html/,
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
    ],
  },
];

function getSERPFollowOnUrl(page) {
  return page + "?s=test&abc=ff&a=foo";
}

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.serpEventTelemetry.enabled", true]],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
  });
});

add_task(async function test_simple_search_page_visit() {
  resetTelemetry();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: getSERPUrl("searchTelemetry.html"),
    },
    async () => {
      await assertSearchSourcesTelemetry(
        {},
        {
          "browser.search.content.unknown": { "example:tagged:ff": 1 },
        }
      );
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
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.TAB_CLOSE,
      },
    },
  ]);
});

add_task(async function test_simple_search_page_visit_telemetry() {
  resetTelemetry();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      /* URL must not be in the cache */
      url: getSERPUrl("searchTelemetry.html") + `&random=${Math.random()}`,
    },
    async () => {
      let scalars = {};
      const key = "browser.search.data_transferred";

      await TestUtils.waitForCondition(() => {
        scalars =
          Services.telemetry.getSnapshotForKeyedScalars("main", false).parent ||
          {};
        return key in scalars;
      }, "should have the expected keyed scalars");

      const scalar = scalars[key];
      Assert.ok("example" in scalar, "correct telemetry category");
      Assert.notEqual(scalars[key].example, 0, "bandwidth logged");
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
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.TAB_CLOSE,
      },
    },
  ]);
});

add_task(async function test_follow_on_visit() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: getSERPFollowOnUrl(getPageUrl()),
    },
    async () => {
      await assertSearchSourcesTelemetry(
        {},
        {
          "browser.search.content.unknown": {
            "example:tagged:ff": 1,
            "example:tagged-follow-on:ff": 1,
          },
        }
      );
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
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.TAB_CLOSE,
      },
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
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.TAB_CLOSE,
      },
    },
  ]);
});

add_task(async function test_track_ad() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd.html")
  );
  await waitForPageWithAdImpressions();

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
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
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_ad_organic() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl("searchTelemetryAd.html", true)
  );
  await waitForPageWithAdImpressions();

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:organic:none": 1 },
      "browser.search.withads.unknown": { "example:organic": 1 },
    }
  );

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
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
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_ad_new_window() {
  resetTelemetry();

  let win = await BrowserTestUtils.openNewBrowserWindow();

  let url = getSERPUrl("searchTelemetryAd.html");
  BrowserTestUtils.startLoadingURIString(win.gBrowser.selectedBrowser, url);
  await BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    false,
    url
  );
  await waitForPageWithAdImpressions();

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
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
  ]);

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_track_ad_pages_without_ads() {
  // Note: the above tests have already checked a page with no ad-urls.
  resetTelemetry();

  let tabs = [];

  tabs.push(
    await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      getSERPUrl("searchTelemetry.html")
    )
  );
  await waitForPageWithAdImpressions();

  tabs.push(
    await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      getSERPUrl("searchTelemetryAd.html")
    )
  );
  await waitForPageWithAdImpressions();

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 2 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
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

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});
