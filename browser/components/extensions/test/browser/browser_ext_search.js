/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_search() {
  const TEST_ID = "test_search@tests.mozilla.com";
  const SEARCH_TERM = "test";
  const SEARCH_URL = "https://localhost/?q={searchTerms}";

  async function background() {
    browser.browserAction.onClicked.addListener(tab => {
      browser.search.search("Search Test", "test", tab.id); // Can't use SEARCH_TERM here
    });
    browser.tabs.onUpdated.addListener(async function(tabId, info, changedTab) {
      if (changedTab.url == "about:blank") {
        // Ignore events related to the initial tab open.
        return;
      }
      if (info.status === "complete") {
        await browser.tabs.remove(tabId);
        browser.test.sendMessage("searchLoaded", changedTab.url);
      }
    });
    await browser.tabs.create({url: "about:blank"});
    let engines = await browser.search.get();
    browser.test.sendMessage("engines", engines);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
      name: TEST_ID,
      "browser_action": {},
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "Search Test",
          "search_url": SEARCH_URL,
        },
      },
    },
    background,
    useAddonManager: "temporary",
  });
  await extension.startup();

  let addonEngines = await extension.awaitMessage("engines");
  let engines = Services.search.getEngines().filter(engine => !engine.hidden);
  is(addonEngines.length, engines.length, "Engine lengths are the same.");
  let defaultEngine = addonEngines.filter(engine => engine.is_default === true);
  is(defaultEngine.length, 1, "One default engine");
  is(defaultEngine[0].name, Services.search.currentEngine.name, "Default engine is correct");
  await clickBrowserAction(extension);
  let url = await extension.awaitMessage("searchLoaded");
  is(url, SEARCH_URL.replace("{searchTerms}", SEARCH_TERM), "Loaded page matches search");
  await extension.unload();
});

add_task(async function test_search_notab() {
  const TEST_ID = "test_search@tests.mozilla.com";
  const SEARCH_TERM = "test";
  const SEARCH_URL = "https://localhost/?q={searchTerms}";

  async function background() {
    browser.browserAction.onClicked.addListener(_ => {
      browser.tabs.onUpdated.addListener(async (tabId, info, changedTab) => {
        if (info.status === "complete") {
          await browser.tabs.remove(tabId);
          browser.test.sendMessage("searchLoaded", changedTab.url);
        }
      });
      browser.search.search("Search Test", "test"); // Can't use SEARCH_TERM here
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
      name: TEST_ID,
      "browser_action": {},
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "Search Test",
          "search_url": SEARCH_URL,
        },
      },
    },
    background,
    useAddonManager: "temporary",
  });
  await extension.startup();

  await clickBrowserAction(extension);
  let url = await extension.awaitMessage("searchLoaded");
  is(url, SEARCH_URL.replace("{searchTerms}", SEARCH_TERM), "Loaded page matches search");
  await extension.unload();
});
