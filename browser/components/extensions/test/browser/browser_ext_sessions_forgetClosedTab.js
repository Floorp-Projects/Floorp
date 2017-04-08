/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_sessions_forget_closed_tab() {
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
            browser.test.assertEq(error.message,
                     `Could not find closed tab using sessionId ${sessionId}.`);
            browser.test.sendMessage("forget-reject");
          }
        );
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions", "tabs"],
    },
    background,
  });

  yield extension.startup();

  let tabUrl = "http://example.com";
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);
  yield BrowserTestUtils.removeTab(tab);
  tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);
  yield BrowserTestUtils.removeTab(tab);

  extension.sendMessage("check-sessions");
  let recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  let recentlyClosedLength = recentlyClosed.length;
  let recentlyClosedTab = recentlyClosed[0].tab;

  // Check that forgetting a tab works properly
  extension.sendMessage("forget-tab", recentlyClosedTab.windowId,
                                      recentlyClosedTab.sessionId);
  yield extension.awaitMessage("forgot-tab");
  extension.sendMessage("check-sessions");
  let remainingClosed = yield extension.awaitMessage("recentlyClosed");
  is(remainingClosed.length, recentlyClosedLength - 1,
     "One tab was forgotten.");
  is(remainingClosed[0].tab.sessionId, recentlyClosed[1].tab.sessionId,
     "The correct tab was forgotten.");

  // Check that re-forgetting the same tab fails properly
  extension.sendMessage("forget-tab", recentlyClosedTab.windowId,
                                      recentlyClosedTab.sessionId);
  yield extension.awaitMessage("forget-reject");
  extension.sendMessage("check-sessions");
  remainingClosed = yield extension.awaitMessage("recentlyClosed");
  is(remainingClosed.length, recentlyClosedLength - 1,
     "No extra tab was forgotten.");
  is(remainingClosed[0].tab.sessionId, recentlyClosed[1].tab.sessionId,
     "The correct tab remains.");

  yield extension.unload();
});
