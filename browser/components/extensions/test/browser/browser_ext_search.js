/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const SEARCH_TERM = "test";
const SEARCH_URL = "https://localhost/?q={searchTerms}";

AddonTestUtils.initMochitest(this);

add_task(async function test_search() {
  async function background(SEARCH_TERM) {
    browser.test.onMessage.addListener(async (msg, tabIds) => {
      if (msg !== "removeTabs") {
        return;
      }

      await browser.tabs.remove(tabIds);
      browser.test.sendMessage("onTabsRemoved");
    });

    function awaitSearchResult() {
      return new Promise(resolve => {
        async function listener(tabId, info, changedTab) {
          if (changedTab.url == "about:blank") {
            // Ignore events related to the initial tab open.
            return;
          }

          if (info.status === "complete") {
            browser.tabs.onUpdated.removeListener(listener);
            resolve({ tabId, url: changedTab.url });
          }
        }

        browser.tabs.onUpdated.addListener(listener);
      });
    }

    let engines = await browser.search.get();
    browser.test.sendMessage("engines", engines);

    // Search with no tabId
    browser.search.search({ query: SEARCH_TERM + "1", engine: "Search Test" });
    let result = await awaitSearchResult();
    browser.test.sendMessage("searchLoaded", result);

    // Search with tabId
    let tab = await browser.tabs.create({});
    browser.search.search({
      query: SEARCH_TERM + "2",
      engine: "Search Test",
      tabId: tab.id,
    });
    result = await awaitSearchResult();
    browser.test.assertEq(result.tabId, tab.id, "Page loaded in right tab");
    browser.test.sendMessage("searchLoaded", result);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["search", "tabs"],
      chrome_settings_overrides: {
        search_provider: {
          name: "Search Test",
          search_url: SEARCH_URL,
        },
      },
    },
    background: `(${background})("${SEARCH_TERM}")`,
    useAddonManager: "temporary",
  });
  await extension.startup();
  await AddonTestUtils.waitForSearchProviderStartup(extension);

  let addonEngines = await extension.awaitMessage("engines");
  let engines = (await Services.search.getEngines()).filter(
    engine => !engine.hidden
  );
  is(addonEngines.length, engines.length, "Engine lengths are the same.");
  let defaultEngine = addonEngines.filter(engine => engine.isDefault === true);
  is(defaultEngine.length, 1, "One default engine");
  is(
    defaultEngine[0].name,
    (await Services.search.getDefault()).name,
    "Default engine is correct"
  );

  const result1 = await extension.awaitMessage("searchLoaded");
  is(
    result1.url,
    SEARCH_URL.replace("{searchTerms}", SEARCH_TERM + "1"),
    "Loaded page matches search"
  );
  await TestUtils.waitForCondition(
    () => !gURLBar.focused,
    "Wait for unfocusing the urlbar"
  );
  info("The urlbar has no focus when searching without tabId");

  const result2 = await extension.awaitMessage("searchLoaded");
  is(
    result2.url,
    SEARCH_URL.replace("{searchTerms}", SEARCH_TERM + "2"),
    "Loaded page matches search"
  );
  await TestUtils.waitForCondition(
    () => !gURLBar.focused,
    "Wait for unfocusing the urlbar"
  );
  info("The urlbar has no focus when searching with tabId");

  extension.sendMessage("removeTabs", [result1.tabId, result2.tabId]);
  await extension.awaitMessage("onTabsRemoved");

  await extension.unload();
});

add_task(async function test_search_default_engine() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["search"],
    },
    background() {
      browser.test.onMessage.addListener((msg, tabId) => {
        browser.test.assertEq(msg, "search");
        browser.search.search({ query: "searchTermForDefaultEngine", tabId });
      });
      browser.test.sendMessage("extension-origin", browser.runtime.getURL("/"));
    },
    useAddonManager: "temporary",
  });

  // Use another extension to intercept and block the search request,
  // so that there is no outbound network activity that would kill the test.
  // This method also allows us to verify that:
  // 1) the search appears as a normal request in the webRequest API.
  // 2) the request is associated with the triggering extension.
  let extensionWithObserver = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["webRequest", "webRequestBlocking", "*://*/*"] },
    async background() {
      let tab = await browser.tabs.create({ url: "about:blank" });
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.log(`Intercepted request ${JSON.stringify(details)}`);
          browser.tabs.remove(tab.id).then(() => {
            browser.test.sendMessage("detectedSearch", details);
          });
          return { cancel: true };
        },
        {
          tabId: tab.id,
          types: ["main_frame"],
          urls: ["*://*/*"],
        },
        ["blocking"]
      );
      browser.test.sendMessage("ready", tab.id);
    },
  });
  await extension.startup();
  const EXPECTED_ORIGIN = await extension.awaitMessage("extension-origin");

  await extensionWithObserver.startup();
  let tabId = await extensionWithObserver.awaitMessage("ready");

  extension.sendMessage("search", tabId);
  let requestDetails = await extensionWithObserver.awaitMessage(
    "detectedSearch"
  );
  await extension.unload();
  await extensionWithObserver.unload();

  ok(
    requestDetails.url.includes("searchTermForDefaultEngine"),
    `Expected search term in ${requestDetails.url}`
  );
  is(
    requestDetails.originUrl,
    EXPECTED_ORIGIN,
    "Search request's should be associated with the originating extension."
  );
});
