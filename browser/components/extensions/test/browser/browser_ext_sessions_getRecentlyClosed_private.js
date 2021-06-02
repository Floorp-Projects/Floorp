/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

SimpleTest.requestCompleteLog();

loadTestSubscript("head_sessions.js");

async function run_test_extension(incognitoOverride) {
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
    incognitoOverride,
  });

  // Open a private browsing window.
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await extension.startup();

  let {
    Management: {
      global: { windowTracker },
    },
  } = ChromeUtils.import("resource://gre/modules/Extension.jsm", null);
  let privateWinId = windowTracker.getId(privateWin);

  extension.sendMessage("check-sessions");
  let recentlyClosed = await extension.awaitMessage("recentlyClosed");
  recordInitialTimestamps(recentlyClosed.map(item => item.lastModified));

  // Open and close two tabs in the private window
  let tab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    "http://example.com"
  );
  BrowserTestUtils.removeTab(tab);

  tab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    "http://example.com"
  );
  let sessionPromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  BrowserTestUtils.removeTab(tab);
  await sessionPromise;

  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  let expectedCount =
    !incognitoOverride || incognitoOverride == "not_allowed" ? 0 : 2;
  checkRecentlyClosed(
    recentlyClosed.filter(onlyNewItemsFilter),
    expectedCount,
    privateWinId,
    true
  );

  // Close the private window.
  await BrowserTestUtils.closeWindow(privateWin);

  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  is(
    recentlyClosed.filter(onlyNewItemsFilter).length,
    0,
    "the closed private window info was not found in recently closed data"
  );

  await extension.unload();
}

add_task(async function test_sessions_get_recently_closed_default() {
  await run_test_extension();
});

add_task(async function test_sessions_get_recently_closed_private_incognito() {
  await run_test_extension("spanning");
  await run_test_extension("not_allowed");
});
