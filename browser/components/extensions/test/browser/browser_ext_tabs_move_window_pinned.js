/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

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
        {url: "<all_urls>"},
        tabs => {
          let destination = tabs[0];
          let source = tabs[1]; // remember, pinning moves it to the left.
          browser.tabs.move(source.id, {windowId: destination.windowId, index: 0});

          browser.tabs.query(
            {url: "<all_urls>"},
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
