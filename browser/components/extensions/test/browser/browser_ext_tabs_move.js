/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:config");

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: async function() {
      let [tab] = await browser.tabs.query({lastFocusedWindow: true});

      browser.tabs.move(tab.id, {index: 0});
      let tabs = await browser.tabs.query({lastFocusedWindow: true});

      browser.test.assertEq(tabs[0].url, tab.url, "should be first tab");
      browser.test.notifyPass("tabs.move.single");
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.single");
  yield extension.unload();

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: async function() {
      let tabs = await browser.tabs.query({lastFocusedWindow: true});

      tabs.sort(function(a, b) { return a.url > b.url; });

      browser.tabs.move(tabs.map(tab => tab.id), {index: 0});

      tabs = await browser.tabs.query({lastFocusedWindow: true});

      browser.test.assertEq(tabs[0].url, "about:blank", "should be first tab");
      browser.test.assertEq(tabs[1].url, "about:config", "should be second tab");
      browser.test.assertEq(tabs[2].url, "about:robots", "should be third tab");

      browser.test.notifyPass("tabs.move.multiple");
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.multiple");
  yield extension.unload();

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    async background() {
      let [, tab] = await browser.tabs.query({lastFocusedWindow: true});

      // Assuming that tab.id of 12345 does not exist.
      await browser.test.assertRejects(
        browser.tabs.move([tab.id, 12345], {index: 0}),
        /Invalid tab/,
        "Should receive invalid tab error");

      let tabs = await browser.tabs.query({lastFocusedWindow: true});
      browser.test.assertEq(tabs[1].url, tab.url, "should be second tab");
      browser.test.notifyPass("tabs.move.invalid");
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.invalid");
  yield extension.unload();

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: async function() {
      let [tab] = await browser.tabs.query({lastFocusedWindow: true});
      browser.tabs.move(tab.id, {index: -1});

      let tabs = await browser.tabs.query({lastFocusedWindow: true});

      browser.test.assertEq(tabs[2].url, tab.url, "should be last tab");
      browser.test.notifyPass("tabs.move.last");
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.last");
  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
});
