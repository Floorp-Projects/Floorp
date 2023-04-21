/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Main tests for SearchSERPTelemetry - general engine visiting and link clicking.
 */

"use strict";

const {
  SearchSERPTelemetry,
  SearchSERPTelemetryUtils,
} = ChromeUtils.importESModule(
  "resource:///modules/SearchSERPTelemetry.sys.mjs"
);

// Note: example.org is used for the SERP page, and example.com is used to serve
// the ads. This is done to simulate different domains like the real servers.
const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp: /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/searchTelemetry(?:Ad)?.html/,
    queryParamName: "s",
    codeParamName: "abc",
    taggedCodes: ["ff"],
    followOnParamNames: ["a"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad2?/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        included: {
          default: true,
        },
      },
    ],
  },
  {
    telemetryId: "example-data-attributes",
    searchPageRegexp: /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/searchTelemetryAd_dataAttributes(?:_none|_href)?.html/,
    queryParamName: "s",
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["xyz"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        included: {
          default: true,
        },
      },
    ],
  },
  {
    telemetryId: "slow-page-load",
    searchPageRegexp: /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/slow_loading_page_with_ads(_on_load_event)?.html/,
    queryParamName: "s",
    codeParamName: "abc",
    taggedCodes: ["ff"],
    followOnParamNames: ["a"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad2?/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        included: {
          default: true,
        },
      },
    ],
  },
];

function getPageUrl(useAdPage = false) {
  let page = useAdPage ? "searchTelemetryAd.html" : "searchTelemetry.html";
  return `https://example.org/browser/browser/components/search/test/browser/${page}`;
}

function getSERPUrl(page, organic = false) {
  return `${page}?s=test${organic ? "" : "&abc=ff"}`;
}

function getSERPFollowOnUrl(page) {
  return page + "?s=test&abc=ff&a=foo";
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

add_task(async function test_simple_search_page_visit() {
  resetTelemetry();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: getSERPUrl(getPageUrl()),
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
    },
  ]);
});

add_task(async function test_simple_search_page_visit_telemetry() {
  resetTelemetry();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      /* URL must not be in the cache */
      url: getSERPUrl(getPageUrl()) + `&random=${Math.random()}`,
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
    },
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);
});

add_task(async function test_track_ad() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl(getPageUrl(true))
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
    }
  );

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
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_ad_on_data_attributes() {
  resetTelemetry();

  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_dataAttributes.html";

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl(url)
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": {
        "example-data-attributes:tagged:ff": 1,
      },
      "browser.search.withads.unknown": {
        "example-data-attributes:tagged": 1,
      },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example-data-attributes",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_ad_on_data_attributes_and_hrefs() {
  resetTelemetry();

  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_dataAttributes_href.html";

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl(url)
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": {
        "example-data-attributes:tagged:ff": 1,
      },
      "browser.search.withads.unknown": {
        "example-data-attributes:tagged": 1,
      },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example-data-attributes",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_no_ad_on_data_attributes_and_hrefs() {
  resetTelemetry();

  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "searchTelemetryAd_dataAttributes_none.html";

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl(url)
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": {
        "example-data-attributes:tagged:ff": 1,
      },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example-data-attributes",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_ad_on_DOMContentLoaded() {
  resetTelemetry();

  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "slow_loading_page_with_ads.html";

  let observeAdPreviouslyRecorded = TestUtils.consoleMessageObserved(msg => {
    return (
      typeof msg.wrappedJSObject.arguments?.[0] == "string" &&
      msg.wrappedJSObject.arguments[0].includes(
        "Ad was previously reported for browser with URI"
      )
    );
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl(url)
  );

  // Observe ad was counted on DOMContentLoaded.
  // We do not count the ad again on load.
  await observeAdPreviouslyRecorded;

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "slow-page-load:tagged:ff": 1 },
      "browser.search.withads.unknown": { "slow-page-load:tagged": 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "slow-page-load",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_ad_on_load_event() {
  resetTelemetry();

  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + "slow_loading_page_with_ads_on_load_event.html";

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl(url)
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "slow-page-load:tagged:ff": 1 },
      "browser.search.withads.unknown": { "slow-page-load:tagged": 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "slow-page-load",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_ad_organic() {
  resetTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl(getPageUrl(true), true)
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:organic:none": 1 },
      "browser.search.withads.unknown": { "example:organic": 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged: "false",
        partner_code: "",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_ad_new_window() {
  resetTelemetry();

  let win = await BrowserTestUtils.openNewBrowserWindow();

  let url = getSERPUrl(getPageUrl(true));
  BrowserTestUtils.loadURIString(win.gBrowser.selectedBrowser, url);
  await BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    false,
    url
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
    }
  );

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
      getSERPUrl(getPageUrl(false))
    )
  );
  tabs.push(
    await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      getSERPUrl(getPageUrl(true))
    )
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 2 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
    }
  );

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
    },
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

async function track_ad_click(testOrganic) {
  // Note: the above tests have already checked a page with no ad-urls.
  resetTelemetry();

  let expectedScalarKey = `example:${testOrganic ? "organic" : "tagged"}`;
  let expectedContentScalarKey = `example:${
    testOrganic ? "organic:none" : "tagged:ff"
  }`;
  let tagged = testOrganic ? "false" : "true";
  let partnerCode = testOrganic ? "" : "ff";

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getSERPUrl(getPageUrl(true), testOrganic)
  );

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { [expectedContentScalarKey]: 1 },
      "browser.search.withads.unknown": {
        [expectedScalarKey.replace("sap", "tagged")]: 1,
      },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);
  await promiseAdImpressionReceived(1);

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.getElementById("ad1").click();
  });
  await pageLoadPromise;
  await promiseWaitForAdLinkCheck();

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { [expectedContentScalarKey]: 1 },
      "browser.search.withads.unknown": { [expectedScalarKey]: 1 },
      "browser.search.adclicks.unknown": { [expectedScalarKey]: 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "unknown",
        is_shopping_page: "false",
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

  // Now go back, and click again.
  pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  gBrowser.goBack();
  await pageLoadPromise;
  await promiseWaitForAdLinkCheck();

  // We've gone back, so we register an extra display & if it is with ads or not.
  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.tabhistory": { [expectedContentScalarKey]: 1 },
      "browser.search.content.unknown": { [expectedContentScalarKey]: 1 },
      "browser.search.withads.tabhistory": { [expectedScalarKey]: 1 },
      "browser.search.withads.unknown": { [expectedScalarKey]: 1 },
      "browser.search.adclicks.unknown": { [expectedScalarKey]: 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "tabhistory",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
    },
  ]);
  await promiseAdImpressionReceived(2);

  pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.getElementById("ad1").click();
  });
  await pageLoadPromise;
  await promiseWaitForAdLinkCheck();

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.tabhistory": { [expectedContentScalarKey]: 1 },
      "browser.search.content.unknown": { [expectedContentScalarKey]: 1 },
      "browser.search.withads.tabhistory": { [expectedScalarKey]: 1 },
      "browser.search.withads.unknown": { [expectedScalarKey]: 1 },
      "browser.search.adclicks.tabhistory": { [expectedScalarKey]: 1 },
      "browser.search.adclicks.unknown": { [expectedScalarKey]: 1 },
    }
  );

  assertImpressionEvents([
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "unknown",
        is_shopping_page: "false",
        shopping_tab_displayed: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
    },
    {
      impression: {
        provider: "example",
        tagged,
        partner_code: partnerCode,
        source: "tabhistory",
        is_shopping_page: "false",
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
}

add_task(async function test_track_ad_click() {
  await track_ad_click(false);
});

add_task(async function test_track_ad_click_organic() {
  await track_ad_click(true);
});

add_task(async function test_track_ad_click_with_location_change_other_tab() {
  resetTelemetry();
  const url = getSERPUrl(getPageUrl(true));
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
    }
  );

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
    },
  ]);
  await promiseAdImpressionReceived();

  const newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );

  await BrowserTestUtils.switchTab(gBrowser, tab);

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.getElementById("ad1").click();
  });
  await pageLoadPromise;

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
      "browser.search.adclicks.unknown": { "example:tagged": 1 },
    }
  );

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
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(tab);
});
