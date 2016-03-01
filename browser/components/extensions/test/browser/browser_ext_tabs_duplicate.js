/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testDuplicateTab() {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query({
        lastFocusedWindow: true,
      }, function(tabs) {
        let source = tabs[1];
        // By moving it 0, we check that the new tab is created next
        // to the existing one.
        browser.tabs.move(source.id, {index: 0}, () => {
          browser.tabs.duplicate(source.id, (tab) => {
            browser.test.assertEq("http://example.net/", tab.url);
            // Should be the second tab, next to the one duplicated.
            browser.test.assertEq(1, tab.index);
            // Should be selected by default.
            browser.test.assertTrue(tab.selected);
            browser.test.notifyPass("tabs.duplicate");
          });
        });
      });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.duplicate");
  yield extension.unload();

  while (gBrowser.tabs[0].linkedBrowser.currentURI.spec === "http://example.net/") {
    yield BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }
});

add_task(function* testDuplicatePinnedTab() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");
  gBrowser.pinTab(tab);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query({
        lastFocusedWindow: true,
      }, function(tabs) {
        // Duplicate the pinned tab, example.net.
        browser.tabs.duplicate(tabs[0].id, (tab) => {
          browser.test.assertEq("http://example.net/", tab.url);
          // Should be the second tab, next to the one duplicated.
          browser.test.assertEq(1, tab.index);
          // Should be pinned.
          browser.test.assertTrue(tab.pinned);
          browser.test.notifyPass("tabs.duplicate.pinned");
        });
      });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.duplicate.pinned");
  yield extension.unload();

  while (gBrowser.tabs[0].linkedBrowser.currentURI.spec === "http://example.net/") {
    yield BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }
});
