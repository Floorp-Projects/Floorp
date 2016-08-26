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

add_task(function* testDuplicateTabLazily() {
  function background() {
    let tabLoadComplete = new Promise(resolve => {
      browser.test.onMessage.addListener((message, tabId, result) => {
        if (message == "duplicate-tab-done") {
          resolve(tabId);
        }
      });
    });

    function awaitLoad(tabId) {
      return new Promise(resolve => {
        browser.tabs.onUpdated.addListener(function listener(tabId_, changed, tab) {
          if (tabId == tabId_ && changed.status == "complete") {
            browser.tabs.onUpdated.removeListener(listener);
            resolve();
          }
        });
      });
    }

    let startTabId;
    let url = "http://example.com/browser/browser/components/extensions/test/browser/file_dummy.html";
    browser.tabs.create({url}, tab => {
      startTabId = tab.id;

      awaitLoad(startTabId).then(() => {
        browser.test.sendMessage("duplicate-tab", startTabId);

        tabLoadComplete.then(unloadedTabId => {
          browser.tabs.get(startTabId, loadedtab => {
            browser.test.assertEq("Dummy test page", loadedtab.title, "Title should be returned for loaded pages");
            browser.test.assertEq("complete", loadedtab.status, "Tab status should be complete for loaded pages");
          });

          browser.tabs.get(unloadedTabId, unloadedtab => {
            browser.test.assertEq("Dummy test page", unloadedtab.title, "Title should be returned after page has been unloaded");
          });

          browser.tabs.remove([tab.id, unloadedTabId]);
          browser.test.notifyPass("tabs.hasCorrectTabTitle");
        });
      }).catch(e => {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("tabs.hasCorrectTabTitle");
      });
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background,
  });

  extension.onMessage("duplicate-tab", tabId => {
    let {Management: {global: {TabManager}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

    let tab = TabManager.getTab(tabId);
    // This is a bit of a hack to load a tab in the background.
    let newTab = gBrowser.duplicateTab(tab, false);

    BrowserTestUtils.waitForEvent(newTab, "SSTabRestored", () => true).then(() => {
      extension.sendMessage("duplicate-tab-done", TabManager.getId(newTab));
    });
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.hasCorrectTabTitle");
  yield extension.unload();
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
