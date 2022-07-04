/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* global gBrowser SessionStore */
"use strict";

const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);
const triggeringPrincipal_base64 = E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;

let lazyTabState = {
  entries: [
    {
      url: "http://example.com/",
      triggeringPrincipal_base64,
      title: "Example Domain",
    },
  ],
};

add_task(async function test_discarded() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "webNavigation"],
    },

    background() {
      browser.webNavigation.onCompleted.addListener(
        async details => {
          browser.test.log(`webNav onCompleted received for ${details.tabId}`);
          let updatedTab = await browser.tabs.get(details.tabId);
          browser.test.assertEq(
            false,
            updatedTab.discarded,
            "lazy to non-lazy update discard property"
          );
          browser.test.notifyPass("test-finished");
        },
        { url: [{ hostContains: "example.com" }] }
      );

      browser.tabs.onCreated.addListener(function(tab) {
        browser.test.assertEq(
          true,
          tab.discarded,
          "non-lazy tab onCreated discard property"
        );
        browser.tabs.update(tab.id, { active: true });
      });
    },
  });

  await extension.startup();

  let testTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    createLazyBrowser: true,
  });
  SessionStore.setTabState(testTab, lazyTabState);

  await extension.awaitFinish("test-finished");
  await extension.unload();

  BrowserTestUtils.removeTab(testTab);
});

// If discard is called immediately after creating a new tab, the new tab may not have loaded,
// and the sessionstore for that tab is not ready for discarding.  The result was a corrupted
// sessionstore for the tab, which when the tab was activated, resulted in a tab with partial
// state.
add_task(async function test_create_then_discard() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "webNavigation"],
    },

    background: async function() {
      let createdTab;

      browser.tabs.onUpdated.addListener((tabId, updatedInfo) => {
        if (!updatedInfo.discarded) {
          return;
        }

        browser.webNavigation.onCompleted.addListener(
          async details => {
            browser.test.assertEq(
              createdTab.id,
              details.tabId,
              "created tab navigation is completed"
            );
            let activeTab = await browser.tabs.get(details.tabId);
            browser.test.assertEq(
              "http://example.com/",
              details.url,
              "created tab url is correct"
            );
            browser.test.assertEq(
              "http://example.com/",
              activeTab.url,
              "created tab url is correct"
            );
            browser.tabs.remove(details.tabId);
            browser.test.notifyPass("test-finished");
          },
          { url: [{ hostContains: "example.com" }] }
        );

        browser.tabs.update(tabId, { active: true });
      });

      createdTab = await browser.tabs.create({
        url: "http://example.com/",
        active: false,
      });
      browser.tabs.discard(createdTab.id);
    },
  });
  await extension.startup();
  await extension.awaitFinish("test-finished");
  await extension.unload();
});

add_task(async function test_create_discarded() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "webNavigation"],
    },

    background() {
      let tabOpts = {
        url: "http://example.com/",
        active: false,
        discarded: true,
        title: "discarded tab",
      };

      browser.webNavigation.onCompleted.addListener(
        async details => {
          let activeTab = await browser.tabs.get(details.tabId);
          browser.test.assertEq(
            tabOpts.url,
            activeTab.url,
            "restored tab url matches active tab url"
          );
          browser.test.assertEq(
            "mochitest index /",
            activeTab.title,
            "restored tab title is correct"
          );
          browser.tabs.remove(details.tabId);
          browser.test.notifyPass("test-finished");
        },
        { url: [{ hostContains: "example.com" }] }
      );

      browser.tabs.onCreated.addListener(tab => {
        browser.test.assertEq(
          tabOpts.active,
          tab.active,
          "lazy tab is not active"
        );
        browser.test.assertEq(
          tabOpts.discarded,
          tab.discarded,
          "lazy tab is discarded"
        );
        browser.test.assertEq(tabOpts.url, tab.url, "lazy tab url is correct");
        browser.test.assertEq(
          tabOpts.title,
          tab.title,
          "lazy tab title is correct"
        );
        browser.tabs.update(tab.id, { active: true });
      });

      browser.tabs.create(tabOpts);
    },
  });
  await extension.startup();
  await extension.awaitFinish("test-finished");
  await extension.unload();
});

add_task(async function test_discarded_private_tab_restored() {
  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",

    background() {
      browser.tabs.onUpdated.addListener(
        async (tabId, changeInfo, tab) => {
          const { active, discarded, incognito } = tab;
          if (!incognito || active || discarded) {
            return;
          }
          await browser.tabs.discard(tabId);
          browser.test.sendMessage("tab-discarded");
        },
        { properties: ["status"] }
      );
    },
  });

  // Open a private browsing window.
  const privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await extension.startup();

  const newTab = await BrowserTestUtils.addTab(
    privateWin.gBrowser,
    "https://example.com/"
  );
  await extension.awaitMessage("tab-discarded");
  is(newTab.getAttribute("pending"), "true", "private tab should be discarded");

  const promiseTabLoaded = BrowserTestUtils.browserLoaded(newTab.linkedBrowser);

  info("Switching to the discarded background tab");
  await BrowserTestUtils.switchTab(privateWin.gBrowser, newTab);

  info("Wait for the restored tab to complete loading");
  await promiseTabLoaded;
  is(
    newTab.hasAttribute("pending"),
    false,
    "discarded private tab should have been restored"
  );

  is(
    newTab.linkedBrowser.currentURI.spec,
    "https://example.com/",
    "Got the expected url on the restored tab"
  );

  await extension.unload();
  await BrowserTestUtils.closeWindow(privateWin);
});

add_task(async function test_update_discarded() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "<all_urls>"],
    },

    background() {
      browser.test.onMessage.addListener(async msg => {
        let [tab] = await browser.tabs.query({ url: "http://example.com/" });
        if (msg == "update") {
          await browser.tabs.update(tab.id, { url: "https://example.com/" });
        } else {
          browser.test.fail(`Unexpected message received: ${msg}`);
        }
      });
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  let lazyTab = BrowserTestUtils.addTab(gBrowser, "http://example.com/", {
    createLazyBrowser: true,
    lazyTabTitle: "Example Domain",
  });

  let tabBrowserInsertedPromise = BrowserTestUtils.waitForEvent(
    lazyTab,
    "TabBrowserInserted"
  );

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [
      {
        message: /Lazy browser prematurely inserted via 'loadURI' property access:/,
        forbid: true,
      },
    ]);
  });

  extension.sendMessage("update");
  await tabBrowserInsertedPromise;

  await BrowserTestUtils.waitForBrowserStateChange(
    lazyTab.linkedBrowser,
    "https://example.com/",
    stateFlags => {
      return (
        stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW &&
        stateFlags & Ci.nsIWebProgressListener.STATE_STOP
      );
    }
  );

  await TestUtils.waitForTick();
  BrowserTestUtils.removeTab(lazyTab);

  await extension.unload();

  SimpleTest.endMonitorConsole();
  await waitForConsole;
});
