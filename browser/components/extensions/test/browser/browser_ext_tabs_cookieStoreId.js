/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function setup() {
  // make sure userContext is enabled.
  return SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true],
  ]});
});

add_task(async function() {
  info("Start testing tabs.create with cookieStoreId");

  let testCases = [
    // No private window
    {privateTab: false, cookieStoreId: null, success: true, expectedCookieStoreId: "firefox-default"},
    {privateTab: false, cookieStoreId: "firefox-default", success: true, expectedCookieStoreId: "firefox-default"},
    {privateTab: false, cookieStoreId: "firefox-container-1", success: true, expectedCookieStoreId: "firefox-container-1"},
    {privateTab: false, cookieStoreId: "firefox-container-2", success: true, expectedCookieStoreId: "firefox-container-2"},
    {privateTab: false, cookieStoreId: "firefox-container-42", failure: "exist"},
    {privateTab: false, cookieStoreId: "firefox-private", failure: "defaultToPrivate"},
    {privateTab: false, cookieStoreId: "wow", failure: "illegal"},

    // Private window
    {privateTab: true, cookieStoreId: null, success: true, expectedCookieStoreId: "firefox-private"},
    {privateTab: true, cookieStoreId: "firefox-private", success: true, expectedCookieStoreId: "firefox-private"},
    {privateTab: true, cookieStoreId: "firefox-default", failure: "privateToDefault"},
    {privateTab: true, cookieStoreId: "firefox-container-1", failure: "privateToDefault"},
    {privateTab: true, cookieStoreId: "wow", failure: "illegal"},
  ];

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs", "cookies"],
    },

    background: function() {
      function testTab(data, tab) {
        browser.test.assertTrue(data.success, "we want a success");
        browser.test.assertTrue(!!tab, "we have a tab");
        browser.test.assertEq(data.expectedCookieStoreId, tab.cookieStoreId, "tab should have the correct cookieStoreId");
      }

      async function runTest(data) {
        try {
          // Tab Creation
          let tab;
          try {
            tab = await browser.tabs.create({
              windowId: data.privateTab ? this.privateWindowId : this.defaultWindowId,
              cookieStoreId: data.cookieStoreId,
            });

            browser.test.assertTrue(!data.failure, "we want a success");
          } catch (error) {
            browser.test.assertTrue(!!data.failure, "we want a failure");

            if (data.failure == "illegal") {
              browser.test.assertTrue(/Illegal cookieStoreId/.test(error.message),
                                      "runtime.lastError should report the expected error message");
            } else if (data.failure == "defaultToPrivate") {
              browser.test.assertTrue("Illegal to set private cookieStorageId in a non private window",
                                      error.message,
                                      "runtime.lastError should report the expected error message");
            } else if (data.failure == "privateToDefault") {
              browser.test.assertTrue("Illegal to set non private cookieStorageId in a private window",
                                      error.message,
                                      "runtime.lastError should report the expected error message");
            } else if (data.failure == "exist") {
              browser.test.assertTrue(/No cookie store exists/.test(error.message),
                                      "runtime.lastError should report the expected error message");
            } else {
              browser.test.fail("The test is broken");
            }

            browser.test.sendMessage("test-done");
            return;
          }

          // Tests for tab creation
          testTab(data, tab);

          {
            // Tests for tab querying
            let [tab] = await browser.tabs.query({
              windowId: data.privateTab ? this.privateWindowId : this.defaultWindowId,
              cookieStoreId: data.cookieStoreId,
            });

            browser.test.assertTrue(tab != undefined, "Tab found!");
            testTab(data, tab);
          }

          let stores = await browser.cookies.getAllCookieStores();

          let store = stores.find(store => store.id === tab.cookieStoreId);
          browser.test.assertTrue(!!store, "We have a store for this tab.");
          browser.test.assertTrue(store.tabIds.includes(tab.id), "tabIds includes this tab.");

          await browser.tabs.remove(tab.id);

          browser.test.sendMessage("test-done");
        } catch (e) {
          browser.test.fail("An exception has been thrown");
        }
      }

      async function initialize() {
        let win = await browser.windows.create({incognito: true});
        this.privateWindowId = win.id;

        win = await browser.windows.create({incognito: false});
        this.defaultWindowId = win.id;

        browser.test.sendMessage("ready");
      }

      async function shutdown() {
        await browser.windows.remove(this.privateWindowId);
        await browser.windows.remove(this.defaultWindowId);
        browser.test.sendMessage("gone");
      }

      // Waiting for messages
      browser.test.onMessage.addListener((msg, data) => {
        if (msg == "be-ready") {
          initialize();
        } else if (msg == "test") {
          runTest(data);
        } else {
          browser.test.assertTrue("finish", msg, "Shutting down");
          shutdown();
        }
      });
    },
  });

  await extension.startup();

  info("Tests must be ready...");
  extension.sendMessage("be-ready");
  await extension.awaitMessage("ready");
  info("Tests are ready to run!");

  for (let test of testCases) {
    info(`test tab.create with cookieStoreId: "${test.cookieStoreId}"`);
    extension.sendMessage("test", test);
    await extension.awaitMessage("test-done");
  }

  info("Waiting for shutting down...");
  extension.sendMessage("finish");
  await extension.awaitMessage("gone");

  await extension.unload();
});

add_task(async function perma_private_browsing_mode() {
  await SpecialPowers.pushPrefEnv({set: [["browser.privatebrowsing.autostart", true]]});

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs", "cookies"],
    },
    async background() {
      await browser.test.assertRejects(
        browser.tabs.create({cookieStoreId: "firefox-container-1"}),
        /Contextual identities are unavailable in permanent private browsing mode/,
        "should refuse to open container tab in existing non-private window");

      let win = await browser.windows.create({});
      browser.test.assertTrue(win.incognito, "New window should be private when perma-PBM is enabled.");
      await browser.test.assertRejects(
        browser.tabs.create({cookieStoreId: "firefox-container-1", windowId: win.id}),
        /Illegal to set non-private cookieStoreId in a private window/,
        "should refuse to open container tab in private browsing window");
      await browser.windows.remove(win.id);

      browser.test.sendMessage("done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
