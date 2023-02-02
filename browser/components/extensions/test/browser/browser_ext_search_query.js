"use strict";

add_task(async function test_query() {
  async function background() {
    let resolvers = {};

    function tabListener(tabId, changeInfo, tab) {
      if (tab.url == "about:blank") {
        // Ignore events related to the initial tab open.
        return;
      }

      if (changeInfo.status === "complete") {
        let query = new URL(tab.url).searchParams.get("q");
        let resolver = resolvers[query];
        browser.test.assertTrue(resolver, `Found resolver for ${tab.url}`);
        browser.test.assertTrue(
          resolver.resolve,
          `${query} was not resolved yet`
        );
        resolver.resolve({
          tabId,
          windowId: tab.windowId,
        });
        resolver.resolve = null; // resolve can be used only once.
      }
    }
    browser.tabs.onUpdated.addListener(tabListener);

    async function awaitSearchResult(args) {
      resolvers[args.text] = {};
      resolvers[args.text].promise = new Promise(
        _resolve => (resolvers[args.text].resolve = _resolve)
      );
      await browser.search.query(args);
      let searchResult = await resolvers[args.text].promise;
      return searchResult;
    }

    const firstTab = await browser.tabs.create({
      active: true,
      url: "about:blank",
    });

    browser.test.log("Search in current tab (testing default disposition)");
    let result = await awaitSearchResult({
      text: "DefaultDisposition",
    });
    browser.test.assertDeepEq(
      {
        tabId: firstTab.id,
        windowId: firstTab.windowId,
      },
      result,
      "Defaults to current tab in current window"
    );

    browser.test.log(
      "Search in current tab (testing explicit disposition CURRENT_TAB)"
    );
    result = await awaitSearchResult({
      text: "CurrentTab",
      disposition: "CURRENT_TAB",
    });
    browser.test.assertDeepEq(
      {
        tabId: firstTab.id,
        windowId: firstTab.windowId,
      },
      result,
      "Query ran in current tab in current window"
    );

    browser.test.log("Search in new tab (testing disposition NEW_TAB)");
    result = await awaitSearchResult({
      text: "NewTab",
      disposition: "NEW_TAB",
    });
    browser.test.assertFalse(
      result.tabId === firstTab.id,
      "Query ran in new tab"
    );
    browser.test.assertEq(
      result.windowId,
      firstTab.windowId,
      "Query ran in current window"
    );
    await browser.tabs.remove(result.tabId); // Cleanup

    browser.test.log("Search in a specific tab (testing property tabId)");
    let newTab = await browser.tabs.create({
      active: false,
      url: "about:blank",
    });
    result = await awaitSearchResult({
      text: "SpecificTab",
      tabId: newTab.id,
    });
    browser.test.assertDeepEq(
      {
        tabId: newTab.id,
        windowId: firstTab.windowId,
      },
      result,
      "Query ran in specific tab in current window"
    );
    await browser.tabs.remove(newTab.id); // Cleanup

    browser.test.log("Search in a new window (testing disposition NEW_WINDOW)");
    result = await awaitSearchResult({
      text: "NewWindow",
      disposition: "NEW_WINDOW",
    });
    browser.test.assertFalse(
      result.windowId === firstTab.windowId,
      "Query ran in new window"
    );
    await browser.windows.remove(result.windowId); // Cleanup
    await browser.tabs.remove(firstTab.id); // Cleanup

    browser.test.log("Make sure tabId and disposition can't be used together");
    await browser.test.assertRejects(
      browser.search.query({
        text: " ",
        tabId: 1,
        disposition: "NEW_WINDOW",
      }),
      "Cannot set both 'disposition' and 'tabId'",
      "Should not be able to set both tabId and disposition"
    );

    browser.test.log("Make sure we reject if an invalid tabId is used");
    await browser.test.assertRejects(
      browser.search.query({
        text: " ",
        tabId: Number.MAX_SAFE_INTEGER,
      }),
      /Invalid tab ID/,
      "Should not be able to set an invalid tabId"
    );

    browser.test.notifyPass("disposition");
  }
  const SEARCH_NAME = "Search Test";
  let searchExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: SEARCH_NAME,
          search_url: "https://example.org/?q={searchTerms}",
        },
      },
    },
    useAddonManager: "temporary",
  });
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["search", "tabs"],
    },
    background,
  });
  // We need to use a fake search engine because
  // these tests aren't allowed to load actual
  // webpages, like google.com for example.
  await searchExtension.startup();
  await Services.search.setDefault(
    Services.search.getEngineByName(SEARCH_NAME),
    Ci.nsISearchService.CHANGE_REASON_ADDON_INSTALL
  );
  await extension.startup();
  await extension.awaitFinish("disposition");
  await extension.unload();
  await searchExtension.unload();
});
