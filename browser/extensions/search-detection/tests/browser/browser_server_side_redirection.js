/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

const TELEMETRY_EVENTS_FILTERS = {
  category: "addonsSearchDetection",
  method: "etld_change",
};

// The search-detection built-in add-on registers dynamic events.
const TELEMETRY_TEST_UTILS_OPTIONS = { clear: true, process: "dynamic" };

const REDIRECT_SJS =
  "browser/browser/extensions/search-detection/tests/browser/redirect.sjs?q={searchTerms}";
// This URL will redirect to `example.net`, which is different than
// `*.example.com`. That will be the final URL of a redirect chain:
// www.example.com -> example.net
const SEARCH_URL_WWW = `https://www.example.com/${REDIRECT_SJS}`;
// This URL will redirect to `www.example.com`, which will create a redirect
// chain with two hops:
// test2.example.com -> www.example.com -> example.net
const SEARCH_URL_TEST2 = `https://test2.example.com/${REDIRECT_SJS}`;
// This URL will redirect to `test2.example.com`, which will create a redirect
// chain with three hops:
// test1.example.com -> test2.example.com -> www.example.com -> example.net
const SEARCH_URL_TEST1 = `https://test1.example.com/${REDIRECT_SJS}`;

const TEST_SEARCH_ENGINE_ADDON_ID = "some@addon-id";
const TEST_SEARCH_ENGINE_ADDON_VERSION = "4.5.6";

const testServerSideRedirect = async ({
  searchURL,
  expectedEvents,
  tabURL,
}) => {
  Services.telemetry.clearEvents();

  const searchEngineName = "test search engine";
  // Load a default search engine because the add-on we are testing here
  // monitors the search engines.
  const searchEngine = ExtensionTestUtils.loadExtension({
    manifest: {
      version: TEST_SEARCH_ENGINE_ADDON_VERSION,
      browser_specific_settings: {
        gecko: { id: TEST_SEARCH_ENGINE_ADDON_ID },
      },
      chrome_settings_overrides: {
        search_provider: {
          name: searchEngineName,
          keyword: "test",
          search_url: searchURL,
        },
      },
    },
    useAddonManager: "temporary",
  });

  await searchEngine.startup();
  ok(
    Services.search.getEngineByName(searchEngineName),
    "test search engine registered"
  );
  await AddonTestUtils.waitForSearchProviderStartup(searchEngine);

  // Simulate a search (with the test search engine) by navigating to it.
  const url = tabURL || searchURL.replace("{searchTerms}", "some terms");
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    // Wait for the tab to be fully loaded.
    let loaded = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.startLoadingURIString(browser, url);
    await loaded;
  });

  await searchEngine.unload();
  ok(
    !Services.search.getEngineByName(searchEngineName),
    "test search engine unregistered"
  );

  TelemetryTestUtils.assertEvents(
    expectedEvents,
    TELEMETRY_EVENTS_FILTERS,
    TELEMETRY_TEST_UTILS_OPTIONS
  );
};

add_task(function test_redirect_final() {
  return testServerSideRedirect({
    // www.example.com -> example.net
    searchURL: SEARCH_URL_WWW,
    expectedEvents: [
      {
        object: "other",
        value: "server",
        extra: {
          addonId: TEST_SEARCH_ENGINE_ADDON_ID,
          addonVersion: TEST_SEARCH_ENGINE_ADDON_VERSION,
          from: "example.com",
          to: "example.net",
        },
      },
    ],
  });
});

add_task(function test_redirect_two_hops() {
  return testServerSideRedirect({
    // test2.example.com -> www.example.com -> example.net
    searchURL: SEARCH_URL_TEST2,
    expectedEvents: [
      {
        object: "other",
        value: "server",
        extra: {
          addonId: TEST_SEARCH_ENGINE_ADDON_ID,
          addonVersion: TEST_SEARCH_ENGINE_ADDON_VERSION,
          from: "example.com",
          to: "example.net",
        },
      },
    ],
  });
});

add_task(function test_redirect_three_hops() {
  return testServerSideRedirect({
    // test1.example.com -> test2.example.com -> www.example.com -> example.net
    searchURL: SEARCH_URL_TEST1,
    expectedEvents: [
      {
        object: "other",
        value: "server",
        extra: {
          addonId: TEST_SEARCH_ENGINE_ADDON_ID,
          addonVersion: TEST_SEARCH_ENGINE_ADDON_VERSION,
          from: "example.com",
          to: "example.net",
        },
      },
    ],
  });
});

add_task(function test_no_event_when_search_engine_not_used() {
  return testServerSideRedirect({
    // www.example.com -> example.net
    searchURL: SEARCH_URL_WWW,
    // We do not expect any events because the user is not using the search
    // engine that was registered.
    tabURL: "http://mochi.test:8888/search?q=foobar",
    expectedEvents: [],
  });
});

add_task(function test_redirect_chain_does_not_start_on_first_request() {
  return testServerSideRedirect({
    // www.example.com -> example.net
    searchURL: SEARCH_URL_WWW,
    // User first navigates to an URL that isn't monitored and will be
    // redirected to another URL that is monitored.
    tabURL: `http://mochi.test:8888/browser/browser/extensions/search-detection/tests/browser/redirect.sjs?q={searchTerms}`,
    expectedEvents: [
      {
        object: "other",
        value: "server",
        extra: {
          addonId: TEST_SEARCH_ENGINE_ADDON_ID,
          addonVersion: TEST_SEARCH_ENGINE_ADDON_VERSION,
          // We expect this and not `mochi.test` because we do not monitor
          // `mochi.test`, only `example.com`, which is coming from the search
          // engine registered in the test setup.
          from: "example.com",
          to: "example.net",
        },
      },
    ],
  });
});

add_task(async function test_two_extensions_reported() {
  Services.telemetry.clearEvents();

  const searchEngines = [];
  for (const [addonId, addonVersion, isDefault] of [
    ["1-addon@guid", "1.2", false],
    ["2-addon@guid", "3.4", true],
  ]) {
    const searchEngine = ExtensionTestUtils.loadExtension({
      manifest: {
        version: addonVersion,
        browser_specific_settings: {
          gecko: { id: addonId },
        },
        chrome_settings_overrides: {
          search_provider: {
            is_default: isDefault,
            name: `test search engine - ${addonId}`,
            keyword: "test",
            search_url: `${SEARCH_URL_WWW}&id=${addonId}`,
          },
        },
      },
      useAddonManager: "temporary",
    });

    await searchEngine.startup();
    await AddonTestUtils.waitForSearchProviderStartup(searchEngine);

    searchEngines.push(searchEngine);
  }

  // Simulate a search by navigating to it.
  const url = SEARCH_URL_WWW.replace("{searchTerms}", "some terms");
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    // Wait for the tab to be fully loaded.
    let loaded = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.startLoadingURIString(browser, url);
    await loaded;
  });

  await Promise.all(searchEngines.map(engine => engine.unload()));

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "other",
        value: "server",
        extra: {
          addonId: "1-addon@guid",
          addonVersion: "1.2",
          from: "example.com",
          to: "example.net",
        },
      },
      {
        object: "other",
        value: "server",
        extra: {
          addonId: "2-addon@guid",
          addonVersion: "3.4",
          from: "example.com",
          to: "example.net",
        },
      },
    ],
    TELEMETRY_EVENTS_FILTERS,
    TELEMETRY_TEST_UTILS_OPTIONS
  );
});
