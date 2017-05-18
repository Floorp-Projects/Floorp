/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

SimpleTest.requestCompleteLog();

Services.scriptloader.loadSubScript(new URL("head_sessions.js", gTestPath).href,
                                    this);

add_task(async function test_sessions_get_recently_closed_private() {
  function background() {
    browser.test.onMessage.addListener((msg, filter) => {
      if (msg == "check-sessions") {
        browser.sessions.getRecentlyClosed(filter).then(recentlyClosed => {
          browser.test.sendMessage("recentlyClosed", recentlyClosed);
        });
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions", "tabs"],
    },
    background,
  });

  // Open a private browsing window.
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});

  await extension.startup();

  let {Management: {global: {windowTracker}}} = Cu.import("resource://gre/modules/Extension.jsm", {});
  let privateWinId = windowTracker.getId(privateWin);

  extension.sendMessage("check-sessions");
  let recentlyClosed = await extension.awaitMessage("recentlyClosed");
  recordInitialTimestamps(recentlyClosed.map(item => item.lastModified));

  // Open and close two tabs in the private window
  let tab = await BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser, "http://example.com");
  await BrowserTestUtils.removeTab(tab);

  tab = await BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser, "http://example.com");
  await BrowserTestUtils.removeTab(tab);

  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  checkRecentlyClosed(recentlyClosed.filter(onlyNewItemsFilter), 2, privateWinId, true);

  // Close the private window.
  await BrowserTestUtils.closeWindow(privateWin);

  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  is(recentlyClosed.filter(onlyNewItemsFilter).length, 0, "the closed private window info was not found in recently closed data");

  await extension.unload();
});
