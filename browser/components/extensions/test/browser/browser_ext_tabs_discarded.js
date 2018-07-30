/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* global gBrowser SessionStore */
"use strict";

const {Utils} = ChromeUtils.import("resource://gre/modules/sessionstore/Utils.jsm", {});
const triggeringPrincipal_base64 = Utils.SERIALIZED_SYSTEMPRINCIPAL;

let lazyTabState = {entries: [{url: "http://example.com/", triggeringPrincipal_base64, title: "Example Domain"}]};

add_task(async function test_discarded() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs", "webNavigation"],
    },

    background: async function() {
      browser.webNavigation.onCompleted.addListener(async (details) => {
        browser.test.log(`webNav onCompleted received for ${details.tabId}`);
        let updatedTab = await browser.tabs.get(details.tabId);
        browser.test.assertEq(false, updatedTab.discarded, "lazy to non-lazy update discard property");
        browser.test.notifyPass("test-finished");
      }, {url: [{hostContains: "example.com"}]});

      browser.tabs.onCreated.addListener(function(tab) {
        browser.test.assertEq(true, tab.discarded, "non-lazy tab onCreated discard property");
        browser.tabs.update(tab.id, {active: true});
      });
    },
  });

  await extension.startup();

  let testTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {createLazyBrowser: true});
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
      "permissions": ["tabs", "webNavigation"],
    },

    background: async function() {
      let createdTab;

      browser.tabs.onUpdated.addListener((tabId, updatedInfo) => {
        if (!updatedInfo.discarded) {
          return;
        }

        browser.webNavigation.onCompleted.addListener(async (details) => {
          browser.test.assertEq(createdTab.id, details.tabId, "created tab navigation is completed");
          let activeTab = await browser.tabs.get(details.tabId);
          browser.test.assertEq("http://example.com/", details.url, "created tab url is correct");
          browser.test.assertEq("http://example.com/", activeTab.url, "created tab url is correct");
          browser.tabs.remove(details.tabId);
          browser.test.notifyPass("test-finished");
        }, {url: [{hostContains: "example.com"}]});

        browser.tabs.update(tabId, {active: true});
      });

      createdTab = await browser.tabs.create({url: "http://example.com/", active: false});
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
      "permissions": ["tabs", "webNavigation"],
    },

    background: async function() {
      let tabOpts = {
        url: "http://example.com/",
        active: false,
        discarded: true,
        title: "discarded tab",
      };

      browser.webNavigation.onCompleted.addListener(async (details) => {
        let activeTab = await browser.tabs.get(details.tabId);
        browser.test.assertEq(tabOpts.url, activeTab.url, "restored tab url matches active tab url");
        browser.test.assertEq("mochitest index /", activeTab.title, "restored tab title is correct");
        browser.tabs.remove(details.tabId);
        browser.test.notifyPass("test-finished");
      }, {url: [{hostContains: "example.com"}]});

      browser.tabs.onCreated.addListener(tab => {
        browser.test.assertEq(tabOpts.active, tab.active, "lazy tab is not active");
        browser.test.assertEq(tabOpts.discarded, tab.discarded, "lazy tab is discarded");
        browser.test.assertEq(tabOpts.url, tab.url, "lazy tab url is correct");
        browser.test.assertEq(tabOpts.title, tab.title, "lazy tab title is correct");
        browser.tabs.update(tab.id, {active: true});
      });

      browser.tabs.create(tabOpts);
    },
  });
  await extension.startup();
  await extension.awaitFinish("test-finished");
  await extension.unload();
});
