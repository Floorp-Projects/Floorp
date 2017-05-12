/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  let window1 = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab(window.gBrowser, "http://example.net/");
  await BrowserTestUtils.openNewForegroundTab(window.gBrowser, "http://example.com/");
  await BrowserTestUtils.openNewForegroundTab(window1.gBrowser, "http://example.net/");
  await BrowserTestUtils.openNewForegroundTab(window1.gBrowser, "http://example.com/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query(
        {url: "<all_urls>"},
        tabs => {
          let move1 = tabs[1];
          let move3 = tabs[3];
          browser.tabs.move([move1.id, move3.id], {index: 0});
          browser.tabs.query(
            {url: "<all_urls>"},
            tabs => {
              browser.test.assertEq(tabs[0].url, move1.url);
              browser.test.assertEq(tabs[2].url, move3.url);
              browser.test.notifyPass("tabs.move.multiple");
            });
        });
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.move.multiple");
  await extension.unload();

  for (let tab of window.gBrowser.tabs) {
    await BrowserTestUtils.removeTab(tab);
  }
  await BrowserTestUtils.closeWindow(window1);
});
