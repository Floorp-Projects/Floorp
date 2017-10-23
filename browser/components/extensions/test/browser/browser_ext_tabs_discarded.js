/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* global gBrowser SessionStore */
"use strict";

const {Utils} = Cu.import("resource://gre/modules/sessionstore/Utils.jsm", {});
const triggeringPrincipal_base64 = Utils.SERIALIZED_SYSTEMPRINCIPAL;

let lazyTabState = {entries: [{url: "http://example.com/", triggeringPrincipal_base64, title: "Example Domain"}]};

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

