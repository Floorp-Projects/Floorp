/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* global gBrowser */
"use strict";

add_task(async function test_discarded() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background: async function() {
      let tabs = await browser.tabs.query({ currentWindow: true });
      tabs.sort((tab1, tab2) => tab1.index - tab2.index);

      async function finishTest() {
        try {
          await browser.tabs.discard(tabs[0].id);
          await browser.tabs.discard(tabs[2].id);
          browser.test.succeed(
            "attempting to discard an already discarded tab or the active tab should not throw error"
          );
        } catch (e) {
          browser.test.fail(
            "attempting to discard an already discarded tab or the active tab should not throw error"
          );
        }
        let discardedTab = await browser.tabs.get(tabs[2].id);
        browser.test.assertEq(
          false,
          discardedTab.discarded,
          "attempting to discard the active tab should not have succeeded"
        );

        await browser.test.assertRejects(
          browser.tabs.discard(999999999),
          /Invalid tab ID/,
          "attempt to discard invalid tabId should throw"
        );
        await browser.test.assertRejects(
          browser.tabs.discard([999999999, tabs[1].id]),
          /Invalid tab ID/,
          "attempt to discard a valid and invalid tabId should throw"
        );
        discardedTab = await browser.tabs.get(tabs[1].id);
        browser.test.assertEq(
          false,
          discardedTab.discarded,
          "tab is still not discarded"
        );

        browser.test.notifyPass("test-finished");
      }

      browser.tabs.onUpdated.addListener(async function(tabId, updatedInfo) {
        if ("discarded" in updatedInfo) {
          browser.test.assertEq(
            tabId,
            tabs[0].id,
            "discarding tab triggered onUpdated"
          );
          let discardedTab = await browser.tabs.get(tabs[0].id);
          browser.test.assertEq(
            true,
            discardedTab.discarded,
            "discarded tab discard property"
          );

          await finishTest();
        }
      });

      browser.tabs.discard(tabs[0].id);
    },
  });

  BrowserTestUtils.loadURI(gBrowser.browsers[0], "http://example.com");
  await BrowserTestUtils.browserLoaded(gBrowser.browsers[0]);
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com"
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com"
  );

  await extension.startup();

  await extension.awaitFinish("test-finished");
  await extension.unload();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
