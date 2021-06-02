/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function getExtension(incognitoOverride) {
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

async function openAndCloseWindow(url = "http://example.com", privateWin) {
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: privateWin,
  });
  let tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, url);
  let sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  await BrowserTestUtils.closeWindow(win);
  await sessionUpdatePromise;
}

add_task(async function test_sessions_forget_closed_window() {
  let extension = getExtension();
  await extension.startup();

  await openAndCloseWindow("about:config");
  await openAndCloseWindow("about:robots");

  extension.sendMessage("check-sessions");
  let recentlyClosed = await extension.awaitMessage("recentlyClosed");
  let recentlyClosedWindow = recentlyClosed[0].window;

  // Check that forgetting a window works properly
  extension.sendMessage("forget-window", recentlyClosedWindow.sessionId);
  await extension.awaitMessage("forgot-window");
  extension.sendMessage("check-sessions");
  let remainingClosed = await extension.awaitMessage("recentlyClosed");
  is(
    remainingClosed.length,
    recentlyClosed.length - 1,
    "One window was forgotten."
  );
  is(
    remainingClosed[0].window.sessionId,
    recentlyClosed[1].window.sessionId,
    "The correct window was forgotten."
  );

  // Check that re-forgetting the same window fails properly
  extension.sendMessage("forget-window", recentlyClosedWindow.sessionId);
  let errMsg = await extension.awaitMessage("forget-reject");
  is(
    errMsg,
    `Could not find closed window using sessionId ${recentlyClosedWindow.sessionId}.`
  );

  extension.sendMessage("check-sessions");
  remainingClosed = await extension.awaitMessage("recentlyClosed");
  is(
    remainingClosed.length,
    recentlyClosed.length - 1,
    "No extra window was forgotten."
  );
  is(
    remainingClosed[0].window.sessionId,
    recentlyClosed[1].window.sessionId,
    "The correct window remains."
  );

  await extension.unload();
});

add_task(async function test_sessions_forget_closed_window_private() {
  let pb_extension = getExtension("spanning");
  await pb_extension.startup();
  let extension = getExtension("not_allowed");
  await extension.startup();

  await openAndCloseWindow("about:config", true);
  await openAndCloseWindow("about:robots", true);

  pb_extension.sendMessage("check-sessions");
  let recentlyClosed = await pb_extension.awaitMessage("recentlyClosed");
  let recentlyClosedWindow = recentlyClosed[0].window;

  extension.sendMessage("forget-window", recentlyClosedWindow.sessionId);
  await extension.awaitMessage("forgot-window");
  extension.sendMessage("check-sessions");
  let remainingClosed = await extension.awaitMessage("recentlyClosed");
  is(
    remainingClosed.length,
    recentlyClosed.length - 1,
    "One window was forgotten."
  );
  ok(!recentlyClosedWindow.incognito, "not an incognito window");

  await extension.unload();
  await pb_extension.unload();
});
