/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const SEARCH_TERM = "test";
const SEARCH_URL = "https://localhost/?q={searchTerms}";

add_task(async function test_search() {
  async function background(SEARCH_TERM) {
    browser.browserAction.onClicked.addListener(tab => {
      browser.search.search({query: SEARCH_TERM,
                             engine: "Search Test",
                             tabId: tab.id});
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
      permissions: ["search", "tabs"],
      "browser_action": {},
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "Search Test",
          "search_url": SEARCH_URL,
        },
      },
    },
    background: `(${background})("${SEARCH_TERM}")`,
    useAddonManager: "temporary",
  });
  await extension.startup();

  let addonEngines = await extension.awaitMessage("engines");
  let engines = Services.search.getEngines().filter(engine => !engine.hidden);
  is(addonEngines.length, engines.length, "Engine lengths are the same.");
  let defaultEngine = addonEngines.filter(engine => engine.isDefault === true);
  is(defaultEngine.length, 1, "One default engine");
  is(defaultEngine[0].name, Services.search.currentEngine.name, "Default engine is correct");
  await clickBrowserAction(extension);
  let url = await extension.awaitMessage("searchLoaded");
  is(url, SEARCH_URL.replace("{searchTerms}", SEARCH_TERM), "Loaded page matches search");
  await extension.unload();
});

add_task(async function test_search_notab() {
  async function background(SEARCH_TERM) {
    browser.browserAction.onClicked.addListener(_ => {
      browser.tabs.onUpdated.addListener(async (tabId, info, changedTab) => {
        if (info.status === "complete") {
          await browser.tabs.remove(tabId);
          browser.test.sendMessage("searchLoaded", changedTab.url);
        }
      });
      browser.search.search({query: SEARCH_TERM, engine: "Search Test"});
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["search", "tabs"],
      "browser_action": {},
      "chrome_settings_overrides": {
        "search_provider": {
          "name": "Search Test",
          "search_url": SEARCH_URL,
        },
      },
    },
    background: `(${background})("${SEARCH_TERM}")`,
    useAddonManager: "temporary",
  });
  await extension.startup();

  await clickBrowserAction(extension);
  let url = await extension.awaitMessage("searchLoaded");
  is(url, SEARCH_URL.replace("{searchTerms}", SEARCH_TERM), "Loaded page matches search");
  await extension.unload();
});

add_task(async function test_search_from_options_page() {
  function optionsScript(SEARCH_TERM) {
    browser.browserAction.onClicked.addListener(_ => {
      browser.search.search({query: SEARCH_TERM, engine: "Search Test"});
    });
    browser.test.sendMessage("options-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["search"],
      browser_action: {},
      chrome_settings_overrides: {
        search_provider: {
          name: "Search Test",
          search_url: SEARCH_URL,
        },
      },
      options_ui: {
        page: "options.html",
      },
    },
    background() {
      browser.runtime.openOptionsPage();
    },
    useAddonManager: "temporary",
    files: {
      "options.html": `<!DOCTYPE html><meta charset="utf-8"><script src="options.js"></script>`,
      "options.js": `(${optionsScript})("${SEARCH_TERM}")`,
    },
  });
  await extension.startup();
  await extension.awaitMessage("options-ready");

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, SEARCH_URL.replace("{searchTerms}", SEARCH_TERM));

  await clickBrowserAction(extension);

  let tab = await tabPromise;
  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_search_from_sidebar() {
  function sidebarScript(SEARCH_TERM) {
    browser.browserAction.onClicked.addListener(_ => {
      browser.search.search({query: SEARCH_TERM, engine: "Search Test"});
    });
    browser.test.sendMessage("sidebar-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["search"],
      browser_action: {},
      chrome_settings_overrides: {
        search_provider: {
          name: "Search Test",
          search_url: SEARCH_URL,
        },
      },
      sidebar_action: {
        default_panel: "sidebar.html",
        open_at_install: true,
      },
    },
    useAddonManager: "temporary",
    background() {
    },
    files: {
      "sidebar.html": `<!DOCTYPE html><meta charset="utf-8"><script src="sidebar.js"></script>`,
      "sidebar.js": `(${sidebarScript})("${SEARCH_TERM}")`,
    },
  });
  await extension.startup();
  await extension.awaitMessage("sidebar-ready");

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, SEARCH_URL.replace("{searchTerms}", SEARCH_TERM));

  await clickBrowserAction(extension);

  let tab = await tabPromise;
  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_search_default_engine() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["search"],
      browser_action: {},
    },
    background() {
      browser.browserAction.onClicked.addListener(tab => {
        browser.search.search({query: "searchTermForDefaultEngine", tabId: tab.id});
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
    manifest: {permissions: ["webRequest", "webRequestBlocking", "*://*/*"]},
    async background() {
      let tab = await browser.tabs.create({url: "about:blank"});
      browser.webRequest.onBeforeRequest.addListener(details => {
        browser.test.log(`Intercepted request ${JSON.stringify(details)}`);
        browser.tabs.remove(tab.id).then(() => {
          browser.test.sendMessage("detectedSearch", details);
        });
        return {cancel: true};
      }, {
        tabId: tab.id,
        types: ["main_frame"],
        urls: ["*://*/*"],
      }, ["blocking"]);
      browser.test.sendMessage("ready");
    },
  });
  await extension.startup();
  const EXPECTED_ORIGIN = await extension.awaitMessage("extension-origin");

  await extensionWithObserver.startup();
  await extensionWithObserver.awaitMessage("ready");

  await clickBrowserAction(extension);
  let requestDetails = await extensionWithObserver.awaitMessage("detectedSearch");
  await extension.unload();
  await extensionWithObserver.unload();

  ok(requestDetails.url.includes("searchTermForDefaultEngine"), `Expected search term in ${requestDetails.url}`);
  is(requestDetails.originUrl, EXPECTED_ORIGIN, "Search request's should be associated with the originating extension.");
});
