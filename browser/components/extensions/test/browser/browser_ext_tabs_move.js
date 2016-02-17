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

    background: function() {
      browser.tabs.query({
        lastFocusedWindow: true,
      }, function(tabs) {
        let tab = tabs[0];
        browser.tabs.move(tab.id, {index: 0});
        browser.tabs.query(
          {lastFocusedWindow: true},
          tabs => {
            browser.test.assertEq(tabs[0].url, tab.url, "should be first tab");
            browser.test.notifyPass("tabs.move.single");
          });
      });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.single");
  yield extension.unload();

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query(
        {lastFocusedWindow: true},
        tabs => {
          tabs.sort(function(a, b) { return a.url > b.url; });
          browser.tabs.move(tabs.map(tab => tab.id), {index: 0});
          browser.tabs.query(
            {lastFocusedWindow: true},
            tabs => {
              browser.test.assertEq(tabs[0].url, "about:blank", "should be first tab");
              browser.test.assertEq(tabs[1].url, "about:config", "should be second tab");
              browser.test.assertEq(tabs[2].url, "about:robots", "should be third tab");
              browser.test.notifyPass("tabs.move.multiple");
            });
        });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.multiple");
  yield extension.unload();

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query(
        {lastFocusedWindow: true},
        tabs => {
          let tab = tabs[0];
          // Assuming that tab.id of 12345 does not exist.
          browser.tabs.move([12345, tab.id], {index: 0});
          browser.tabs.query(
            {lastFocusedWindow: true},
            tabs => {
              browser.test.assertEq(tabs[0].url, tab.url, "should be first tab");
              browser.test.notifyPass("tabs.move.invalid");
            });
        });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.invalid");
  yield extension.unload();

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query(
        {lastFocusedWindow: true},
        tabs => {
          let tab = tabs[0];
          browser.tabs.move(tab.id, {index: -1});
          browser.tabs.query(
            {lastFocusedWindow: true},
            tabs => {
              browser.test.assertEq(tabs[2].url, tab.url, "should be last tab");
              browser.test.notifyPass("tabs.move.last");
            });
        });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.last");
  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
});
