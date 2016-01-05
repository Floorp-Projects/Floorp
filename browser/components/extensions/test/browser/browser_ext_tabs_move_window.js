/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");
  let window1 = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.openNewForegroundTab(window1.gBrowser, "http://example.com/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query({
        url: "<all_urls>",
      }, function(tabs) {
        let destination = tabs[0];
        let source = tabs[1]; // skip over about:blank in window1
        browser.tabs.move(source.id, {windowId: destination.windowId, index: 0});

        browser.tabs.query(
          { url: "<all_urls>" },
          tabs => {
            browser.test.assertEq(tabs[0].url, "http://example.com/");
            browser.test.assertEq(tabs[0].windowId, destination.windowId);
            browser.test.notifyPass("tabs.move.window");
          });
      });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.window");
  yield extension.unload();

  for (let tab of window.gBrowser.tabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  yield BrowserTestUtils.closeWindow(window1);
});

add_task(function* () {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");
  let window1 = yield BrowserTestUtils.openNewBrowserWindow();
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(window1.gBrowser, "http://example.com/");
  window1.gBrowser.pinTab(tab1);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query(
        { url: "<all_urls>" },
        tabs => {
          let destination = tabs[0];
          let source = tabs[1]; // remember, pinning moves it to the left.
          browser.tabs.move(source.id, {windowId: destination.windowId, index: 0});

          browser.tabs.query(
            { url: "<all_urls>" },
            tabs => {
              browser.test.assertEq(true, tabs[0].pinned);
              browser.test.notifyPass("tabs.move.pin");
            });
        });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.pin");
  yield extension.unload();

  for (let tab of window.gBrowser.tabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  yield BrowserTestUtils.closeWindow(window1);
});

add_task(function* () {
  let window1 = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.openNewForegroundTab(window.gBrowser, "http://example.net/");
  yield BrowserTestUtils.openNewForegroundTab(window.gBrowser, "http://example.com/");
  yield BrowserTestUtils.openNewForegroundTab(window1.gBrowser, "http://example.net/");
  yield BrowserTestUtils.openNewForegroundTab(window1.gBrowser, "http://example.com/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query(
        { url: "<all_urls>" },
        tabs => {
          let move1 = tabs[1];
          let move3 = tabs[3];
          browser.tabs.move([move1.id, move3.id], {index: 0});
          browser.tabs.query(
            { url: "<all_urls>" },
            tabs => {
              browser.test.assertEq(tabs[0].url, move1.url);
              browser.test.assertEq(tabs[2].url, move3.url);
              browser.test.notifyPass("tabs.move.multiple");
            });
        });
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move.multiple");
  yield extension.unload();

  for (let tab of window.gBrowser.tabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  yield BrowserTestUtils.closeWindow(window1);
});
