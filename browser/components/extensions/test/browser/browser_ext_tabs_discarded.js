/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* global gBrowser SessionStore */
"use strict";

let lazyTabState = {entries: [{url: "http://example.com/", title: "Example Domain"}]};

add_task(async function test_discarded() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: async function() {
      let onCreatedTabData = [];
      let discardedEventData = [];

      async function finishTest() {
        browser.test.assertEq(0, discardedEventData.length, "number of discarded events fired");

        onCreatedTabData.sort((data1, data2) => data1.index - data2.index);
        browser.test.assertEq(false, onCreatedTabData[0].discarded, "non-lazy tab onCreated discard property");
        browser.test.assertEq(true, onCreatedTabData[1].discarded, "lazy tab onCreated discard property");

        let tabs = await browser.tabs.query({currentWindow: true});
        tabs.sort((tab1, tab2) => tab1.index - tab2.index);

        browser.test.assertEq(false, tabs[1].discarded, "non-lazy tab query discard property");
        browser.test.assertEq(true, tabs[2].discarded, "lazy tab query discard property");

        let updatedTab = await browser.tabs.update(tabs[2].id, {active: true});
        browser.test.assertEq(false, updatedTab.discarded, "lazy to non-lazy update discard property");
        browser.test.assertEq(false, discardedEventData[0], "lazy to non-lazy onUpdated discard property");

        await browser.tabs.update(tabs[1].id, {active: true});
        await browser.tabs.discard(tabs[2].id);
        let discardedTab = await browser.tabs.get(tabs[2].id);
        browser.test.assertEq(true, discardedTab.discarded, "discarded tab discard property");
        browser.test.assertEq(true, discardedEventData[1], "discarded tab onUpdated discard property");

        try {
          await browser.tabs.discard(tabs[2].id);
          browser.test.succeed("attempting to discard an already discarded tab should not throw error");
        } catch (e) {
          browser.test.fail("attempting to discard an already discarded tab should not throw error");
        }

        await browser.tabs.update(tabs[2].id, {active: true});
        await browser.tabs.update(tabs[1].id, {active: true});
        await browser.test.assertRejects(browser.tabs.discard(999999999), /Invalid tab ID/,
          "attempt to discard invalid tabId should throw");
        await browser.test.assertRejects(browser.tabs.discard([999999999, tabs[2].id]), /Invalid tab ID/,
          "attempt to discard a valid and invalid tabId should throw");
        discardedTab = await browser.tabs.get(tabs[2].id);
        browser.test.assertEq(false, discardedTab.discarded, "tab is still not discarded");

        browser.test.notifyPass("test-finished");
      }

      browser.tabs.onUpdated.addListener(function(tabId, updatedInfo) {
        if ("discarded" in updatedInfo) {
          discardedEventData.push(updatedInfo.discarded);
        }
      });

      browser.tabs.onCreated.addListener(function(tab) {
        onCreatedTabData.push({discarded: tab.discarded, index: tab.index});
        if (onCreatedTabData.length == 2) {
          finishTest();
        }
      });
    },
  });

  await extension.startup();

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");

  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank", {createLazyBrowser: true});
  SessionStore.setTabState(tab2, JSON.stringify(lazyTabState));

  await extension.awaitFinish("test-finished");
  await extension.unload();

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
});

