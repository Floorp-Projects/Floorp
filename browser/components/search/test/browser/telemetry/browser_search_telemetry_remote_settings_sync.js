/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * When Remote Settings receives an update to search-telemetry-v2, we should
 * trigger an update within SearchSERPTelemetry and SearchSERPTelemetryChild
 * without requiring a user to restart their browser.
 */

requestLongerTimeout(5);

ChromeUtils.defineESModuleGetters(this, {
  ADLINK_CHECK_TIMEOUT_MS: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  SEARCH_TELEMETRY_SHARED: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPCategorization: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPDomainToCategoriesMap:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  TELEMETRY_SETTINGS_KEY: "resource:///modules/SearchSERPTelemetry.sys.mjs",
});

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

const TEST_PROVIDER_BROKEN_VARIANT = [
  {
    ...TEST_PROVIDER_INFO[0],
    queryParamNames: ["foo"],
  },
];

const RECORDS = {
  current: TEST_PROVIDER_INFO,
  created: [],
  updated: TEST_PROVIDER_INFO,
  deleted: [],
};

const BROKEN_VARIANT_RECORDS = {
  current: TEST_PROVIDER_BROKEN_VARIANT,
  created: [],
  updated: TEST_PROVIDER_BROKEN_VARIANT,
  deleted: [],
};

const client = RemoteSettings(TELEMETRY_SETTINGS_KEY);
const db = client.db;
let record = TEST_PROVIDER_INFO[0];

async function updateClientWithRecords(records) {
  let promise = TestUtils.topicObserved("search-telemetry-v2-synced");

  await client.emit("sync", { data: records });

  info("Wait for SearchSERPTelemetry to update.");
  await promise;
}

add_setup(async function () {
  // Initialize the test with a variant of telemetry that won't trigger an
  // impression due to an odd query param name.
  SearchSERPTelemetry.overrideSearchTelemetryForTests(
    TEST_PROVIDER_BROKEN_VARIANT
  );
  await waitForIdle();
  // Enable local telemetry recording for the duration of the tests.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.serpEventTelemetry.enabled", true],
      // Set the IPC count to a small number so that we only have to open
      // one additional tab to reuse the same process.
      ["dom.ipc.processCount.webIsolated", 1],
    ],
  });

  // Shorten the time it takes to examine pages for ads.
  Services.ppmm.sharedData.set(SEARCH_TELEMETRY_SHARED.LOAD_TIMEOUT, 500);
  Services.ppmm.sharedData.flush();

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
    await db.clear();
    await SpecialPowers.popPrefEnv();
    Services.ppmm.sharedData.set(
      SEARCH_TELEMETRY_SHARED.LOAD_TIMEOUT,
      ADLINK_CHECK_TIMEOUT_MS
    );
    Services.ppmm.sharedData.flush();
  });
});

add_task(async function update_telemetry_tab_already_open() {
  info("Load SERP in a new tab.");
  let url = getSERPUrl("searchTelemetryAd.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  info("Wait a brief amount of time for a possible SERP impression.");
  await waitForIdle();

  info("Assert no impressions are found.");
  assertSERPTelemetry([]);

  info("Update search-telemetry-v2 with a matching queryParamName.");
  await updateClientWithRecords(RECORDS);

  info("Reload page.");
  gBrowser.reload();
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await waitForPageWithAdImpressions();

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "reload",
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

  // Change search-telemetry-v2 back to the broken variant so that the next
  // test can check updating the collection while no tabs are open results
  // in a SERP check.
  info("Update search-telemetry-v2 with non-matching queryParamName.");
  await updateClientWithRecords(BROKEN_VARIANT_RECORDS);

  info("Remove tab and reset telemetry.");
  await BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});

add_task(async function update_telemetry_tab_closed() {
  info("Load SERP in a new tab.");
  let url = getSERPUrl("searchTelemetryAd.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  info("Wait a brief amount of time for a possible SERP impression.");
  await waitForIdle();

  info("Assert no impressions are found.");
  assertSERPTelemetry([]);

  info("Remove tab.");
  await BrowserTestUtils.removeTab(tab);

  info("Update search-telemetry-v2 with a matching queryParamName.");
  await updateClientWithRecords(RECORDS);

  info("Load SERP in a new tab.");
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();
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

  info("Update search-telemetry-v2 with non-matching queryParamName.");
  await updateClientWithRecords(BROKEN_VARIANT_RECORDS);

  info("Remove tab and reset telemetry.");
  await BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});

add_task(async function update_telemetry_multiple_tabs() {
  info("Load SERP in a new tab.");
  let url = getSERPUrl("searchTelemetryAd.html");

  let tabs = [];
  for (let index = 0; index < 5; ++index) {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    tabs.push(tab);
  }

  info("Wait a brief amount of time for a possible SERP impression.");
  await waitForIdle();

  info("Assert no impressions are found.");
  assertSERPTelemetry([]);

  info("Update search-telemetry-v2 with a matching queryParamName.");
  await updateClientWithRecords(RECORDS);

  for (let tab of tabs) {
    await BrowserTestUtils.switchTab(gBrowser, tab);
    gBrowser.reload();
    await waitForPageWithAdImpressions();

    assertSERPTelemetry([
      {
        impression: {
          provider: "example",
          tagged: "true",
          partner_code: "ff",
          source: "reload",
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
    await BrowserTestUtils.removeTab(tab);
    resetTelemetry();
  }

  info("Update search-telemetry-v2 with non-matching queryParamName.");
  await updateClientWithRecords(BROKEN_VARIANT_RECORDS);
});

add_task(async function update_telemetry_multiple_processes_and_tabs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Set the IPC count to a higher number to allow for multiple processes
      // for the same domain to be available.
      ["dom.ipc.processCount.webIsolated", 4],
    ],
  });

  info("Load SERP in a new tab.");
  let url = getSERPUrl("searchTelemetryAd.html");

  let tabs = [];
  for (let index = 0; index < 8; ++index) {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    tabs.push(tab);
  }

  info("Wait a brief amount of time for a possible SERP impression.");
  await waitForIdle();

  info("Assert no impressions are found.");
  assertSERPTelemetry([]);

  info("Update search-telemetry-v2 with a matching queryParamName.");
  await updateClientWithRecords(RECORDS);

  for (let tab of tabs) {
    await BrowserTestUtils.switchTab(gBrowser, tab);
    gBrowser.reload();
    await waitForPageWithAdImpressions();

    assertSERPTelemetry([
      {
        impression: {
          provider: "example",
          tagged: "true",
          partner_code: "ff",
          source: "reload",
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

    await BrowserTestUtils.removeTab(tab);
    resetTelemetry();
  }

  info("Update search-telemetry-v2 with non-matching queryParamName.");
  await updateClientWithRecords(BROKEN_VARIANT_RECORDS);
  await SpecialPowers.popPrefEnv();
});
