/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function getExtension(incognitoOverride) {
  function background() {
    browser.test.onMessage.addListener((msg, windowId, sessionId) => {
      if (msg === "check-sessions") {
        browser.sessions.getRecentlyClosed().then(recentlyClosed => {
          browser.test.sendMessage("recentlyClosed", recentlyClosed);
        });
      } else if (msg === "forget-tab") {
        browser.sessions.forgetClosedTab(windowId, sessionId).then(
          () => {
            browser.test.sendMessage("forgot-tab");
          },
          error => {
            browser.test.sendMessage("forget-reject", error.message);
          }
        );
      }
    });
  }

  return ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions", "tabs"],
    },
    background,
    incognitoOverride,
  });
}

add_task(async function test_sessions_forget_closed_tab() {
  let extension = getExtension();
  await extension.startup();

  let tabUrl = "http://example.com";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);
  BrowserTestUtils.removeTab(tab);
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);
  let sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  BrowserTestUtils.removeTab(tab);
  await sessionUpdatePromise;

  extension.sendMessage("check-sessions");
  let recentlyClosed = await extension.awaitMessage("recentlyClosed");
  let recentlyClosedLength = recentlyClosed.length;
  let recentlyClosedTab = recentlyClosed[0].tab;

  // Check that forgetting a tab works properly
  extension.sendMessage(
    "forget-tab",
    recentlyClosedTab.windowId,
    recentlyClosedTab.sessionId
  );
  await extension.awaitMessage("forgot-tab");
  extension.sendMessage("check-sessions");
  let remainingClosed = await extension.awaitMessage("recentlyClosed");
  is(
    remainingClosed.length,
    recentlyClosedLength - 1,
    "One tab was forgotten."
  );
  is(
    remainingClosed[0].tab.sessionId,
    recentlyClosed[1].tab.sessionId,
    "The correct tab was forgotten."
  );

  // Check that re-forgetting the same tab fails properly
  extension.sendMessage(
    "forget-tab",
    recentlyClosedTab.windowId,
    recentlyClosedTab.sessionId
  );
  let errormsg = await extension.awaitMessage("forget-reject");
  is(
    errormsg,
    `Could not find closed tab using sessionId ${recentlyClosedTab.sessionId}.`
  );

  extension.sendMessage("check-sessions");
  remainingClosed = await extension.awaitMessage("recentlyClosed");
  is(
    remainingClosed.length,
    recentlyClosedLength - 1,
    "No extra tab was forgotten."
  );
  is(
    remainingClosed[0].tab.sessionId,
    recentlyClosed[1].tab.sessionId,
    "The correct tab remains."
  );

  await extension.unload();
});

add_task(async function test_sessions_forget_closed_tab_private() {
  SpecialPowers.pushPrefEnv({
    set: [["extensions.allowPrivateBrowsingByDefault", false]],
  });

  let pb_extension = getExtension("spanning");
  await pb_extension.startup();
  let extension = getExtension();
  await extension.startup();

  // Open a private browsing window.
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let tabUrl = "http://example.com";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    tabUrl
  );
  BrowserTestUtils.removeTab(tab);
  tab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    tabUrl
  );
  let sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  BrowserTestUtils.removeTab(tab);
  await sessionUpdatePromise;

  pb_extension.sendMessage("check-sessions");
  let recentlyClosed = await pb_extension.awaitMessage("recentlyClosed");
  let recentlyClosedTab = recentlyClosed[0].tab;

  // Check that forgetting a tab works properly
  extension.sendMessage(
    "forget-tab",
    recentlyClosedTab.windowId,
    recentlyClosedTab.sessionId
  );
  let errormsg = await extension.awaitMessage("forget-reject");
  ok(/Invalid window ID/.test(errormsg), "could not access window");

  await BrowserTestUtils.closeWindow(privateWin);
  await extension.unload();
  await pb_extension.unload();
});
