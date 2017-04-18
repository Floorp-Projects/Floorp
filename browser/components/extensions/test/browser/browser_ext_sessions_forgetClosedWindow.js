/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_sessions_forget_closed_window() {
  function* openAndCloseWindow(url = "http://example.com") {
    let win = yield BrowserTestUtils.openNewBrowserWindow();
    yield BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, url);
    yield BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    yield BrowserTestUtils.closeWindow(win);
  }

  function background() {
    browser.test.onMessage.addListener((msg, sessionId) => {
      if (msg === "check-sessions") {
        browser.sessions.getRecentlyClosed().then(recentlyClosed => {
          browser.test.sendMessage("recentlyClosed", recentlyClosed);
        });
      } else if (msg === "forget-window") {
        browser.sessions.forgetClosedWindow(sessionId).then(
          () => {
            browser.test.sendMessage("forgot-window");
          },
          error => {
            browser.test.assertEq(error.message,
                  `Could not find closed window using sessionId ${sessionId}.`);
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

  yield openAndCloseWindow("about:config");
  yield openAndCloseWindow("about:robots");

  extension.sendMessage("check-sessions");
  let recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  let recentlyClosedLength = recentlyClosed.length;
  let recentlyClosedWindow = recentlyClosed[0].window;

  // Check that forgetting a window works properly
  extension.sendMessage("forget-window", recentlyClosedWindow.sessionId);
  yield extension.awaitMessage("forgot-window");
  extension.sendMessage("check-sessions");
  let remainingClosed = yield extension.awaitMessage("recentlyClosed");
  is(remainingClosed.length, recentlyClosedLength - 1,
     "One window was forgotten.");
  is(remainingClosed[0].window.sessionId, recentlyClosed[1].window.sessionId,
     "The correct window was forgotten.");

  // Check that re-forgetting the same window fails properly
  extension.sendMessage("forget-window", recentlyClosedWindow.sessionId);
  yield extension.awaitMessage("forget-reject");
  extension.sendMessage("check-sessions");
  remainingClosed = yield extension.awaitMessage("recentlyClosed");
  is(remainingClosed.length, recentlyClosedLength - 1,
     "No extra window was forgotten.");
  is(remainingClosed[0].window.sessionId, recentlyClosed[1].window.sessionId,
     "The correct window remains.");

  yield extension.unload();
});
