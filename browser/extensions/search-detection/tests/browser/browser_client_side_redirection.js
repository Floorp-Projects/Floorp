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

async function testClientSideRedirect({
  background,
  permissions,
  telemetryExpected = false,
}) {
  Services.telemetry.clearEvents();

  // Load an extension that does a client-side redirect. We expect this
  // extension to be reported in a Telemetry event when `telemetryExpected` is
  // set to `true`.
  const addonId = "some@addon-id";
  const addonVersion = "1.2.3";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      version: addonVersion,
      browser_specific_settings: { gecko: { id: addonId } },
      permissions,
    },
    useAddonManager: "temporary",
    background,
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  // Simulate a search (with the test search engine) by navigating to it.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com/search?q=babar",
    },
    () => {}
  );

  await extension.unload();

  TelemetryTestUtils.assertEvents(
    telemetryExpected
      ? [
          {
            object: "webrequest",
            value: "extension",
            extra: {
              addonId,
              addonVersion,
              from: "example.com",
              to: "mochi.test",
            },
          },
        ]
      : [],
    TELEMETRY_EVENTS_FILTERS,
    TELEMETRY_TEST_UTILS_OPTIONS
  );
}

add_setup(async function () {
  const searchEngineName = "test search engine";

  let searchEngine;

  // This cleanup function has to be registered before the one registered
  // internally by loadExtension, otherwise it is going to trigger a test
  // failure (because it will be called too late).
  registerCleanupFunction(async () => {
    await searchEngine.unload();
    ok(
      !Services.search.getEngineByName(searchEngineName),
      "test search engine unregistered"
    );
  });

  searchEngine = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: searchEngineName,
          keyword: "test",
          search_url: "https://example.com/?q={searchTerms}",
        },
      },
    },
    // NOTE: the search extension needs to be installed through the
    // AddonManager to be correctly unregistered when it is uninstalled.
    useAddonManager: "temporary",
  });

  await searchEngine.startup();
  await AddonTestUtils.waitForSearchProviderStartup(searchEngine);
  ok(
    Services.search.getEngineByName(searchEngineName),
    "test search engine registered"
  );
});

add_task(function test_onBeforeRequest() {
  return testClientSideRedirect({
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        () => {
          return {
            redirectUrl: "http://mochi.test:8888/",
          };
        },
        { urls: ["*://example.com/*"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
    permissions: ["webRequest", "webRequestBlocking", "*://example.com/*"],
    telemetryExpected: true,
  });
});

add_task(function test_onBeforeRequest_url_not_monitored() {
  // Here, we load an extension that does a client-side redirect. Because this
  // extension does not listen to the URL of the search engine registered
  // above, we don't expect this extension to be reported in a Telemetry event.
  return testClientSideRedirect({
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        () => {
          return {
            redirectUrl: "http://mochi.test:8888/",
          };
        },
        { urls: ["*://google.com/*"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
    permissions: ["webRequest", "webRequestBlocking", "*://google.com/*"],
    telemetryExpected: false,
  });
});

add_task(function test_onHeadersReceived() {
  return testClientSideRedirect({
    background() {
      browser.webRequest.onHeadersReceived.addListener(
        () => {
          return {
            redirectUrl: "http://mochi.test:8888/",
          };
        },
        { urls: ["*://example.com/*"], types: ["main_frame"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
    permissions: ["webRequest", "webRequestBlocking", "*://example.com/*"],
    telemetryExpected: true,
  });
});

add_task(function test_onHeadersReceived_url_not_monitored() {
  // Here, we load an extension that does a client-side redirect. Because this
  // extension does not listen to the URL of the search engine registered
  // above, we don't expect this extension to be reported in a Telemetry event.
  return testClientSideRedirect({
    background() {
      browser.webRequest.onHeadersReceived.addListener(
        () => {
          return {
            redirectUrl: "http://mochi.test:8888/",
          };
        },
        { urls: ["*://google.com/*"], types: ["main_frame"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
    permissions: ["webRequest", "webRequestBlocking", "*://google.com/*"],
    telemetryExpected: false,
  });
});
