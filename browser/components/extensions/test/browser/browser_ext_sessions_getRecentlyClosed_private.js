/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals recordInitialTimestamps, onlyNewItemsFilter, checkRecentlyClosed */

SimpleTest.requestCompleteLog();

Services.scriptloader.loadSubScript(new URL("head_sessions.js", gTestPath).href,
                                    this);

add_task(function* test_sessions_get_recently_closed_private() {
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
  let privateWin = yield BrowserTestUtils.openNewBrowserWindow({private: true});

  yield extension.startup();

  let {Management: {global: {WindowManager}}} = Cu.import("resource://gre/modules/Extension.jsm", {});
  let privateWinId = WindowManager.getId(privateWin);

  extension.sendMessage("check-sessions");
  let recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  recordInitialTimestamps(recentlyClosed.map(item => item.lastModified));

  // Open and close two tabs in the private window
  let tab = yield BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser, "http://example.com");
  yield BrowserTestUtils.removeTab(tab);

  tab = yield BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser, "http://example.com");
  yield BrowserTestUtils.removeTab(tab);

  extension.sendMessage("check-sessions");
  recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  checkRecentlyClosed(recentlyClosed.filter(onlyNewItemsFilter), 2, privateWinId, true);

  // Close the private window.
  yield BrowserTestUtils.closeWindow(privateWin);

  extension.sendMessage("check-sessions");
  recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  is(recentlyClosed.filter(onlyNewItemsFilter).length, 0, "the closed private window info was not found in recently closed data");

  yield extension.unload();
});
