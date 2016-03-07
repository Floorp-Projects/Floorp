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
          {url: "<all_urls>"},
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
