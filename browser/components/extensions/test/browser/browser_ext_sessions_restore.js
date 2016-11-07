/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

SimpleTest.requestCompleteLog();

XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
                                  "resource:///modules/sessionstore/SessionStore.jsm");

add_task(function* test_sessions_restore() {
  function background() {
    browser.test.onMessage.addListener((msg, data) => {
      if (msg == "check-sessions") {
        browser.sessions.getRecentlyClosed().then(recentlyClosed => {
          browser.test.sendMessage("recentlyClosed", recentlyClosed);
        });
      } else if (msg == "restore") {
        browser.sessions.restore(data).then(sessions => {
          browser.test.sendMessage("restored", sessions);
        });
      } else if (msg == "restore-reject") {
        browser.sessions.restore("not-a-valid-session-id").then(
          sessions => {
            browser.test.fail("restore rejected with an invalid sessionId");
          },
          error => {
            browser.test.assertTrue(
              error.message.includes("Could not restore object using sessionId not-a-valid-session-id."));
            browser.test.sendMessage("restore-rejected");
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

  let {Management: {global: {WindowManager, TabManager}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  function checkLocalTab(tab, expectedUrl) {
    let realTab = TabManager.getTab(tab.id);
    let tabState = JSON.parse(SessionStore.getTabState(realTab));
    is(tabState.entries[0].url, expectedUrl, "restored tab has the expected url");
  }

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, "about:config");
  yield BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  for (let url of ["about:robots", "about:mozilla"]) {
    yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, url);
  }
  yield BrowserTestUtils.closeWindow(win);

  extension.sendMessage("check-sessions");
  let recentlyClosed = yield extension.awaitMessage("recentlyClosed");

  // Check that our expected window is the most recently closed.
  is(recentlyClosed[0].window.tabs.length, 3, "most recently closed window has the expected number of tabs");

  // Restore the window.
  extension.sendMessage("restore");
  let restored = yield extension.awaitMessage("restored");

  is(restored.length, 1, "restore returned the expected number of sessions");
  is(restored[0].window.tabs.length, 3, "restore returned a window with the expected number of tabs");
  checkLocalTab(restored[0].window.tabs[0], "about:config");
  checkLocalTab(restored[0].window.tabs[1], "about:robots");
  checkLocalTab(restored[0].window.tabs[2], "about:mozilla");

  // Close the window again.
  let window = WindowManager.getWindow(restored[0].window.id);
  yield BrowserTestUtils.closeWindow(window);

  // Restore the window using the sessionId.
  extension.sendMessage("check-sessions");
  recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  extension.sendMessage("restore", recentlyClosed[0].window.sessionId);
  restored = yield extension.awaitMessage("restored");

  is(restored.length, 1, "restore returned the expected number of sessions");
  is(restored[0].window.tabs.length, 3, "restore returned a window with the expected number of tabs");
  checkLocalTab(restored[0].window.tabs[0], "about:config");
  checkLocalTab(restored[0].window.tabs[1], "about:robots");
  checkLocalTab(restored[0].window.tabs[2], "about:mozilla");

  // Close the window again.
  window = WindowManager.getWindow(restored[0].window.id);
  yield BrowserTestUtils.closeWindow(window);

  // Open and close a tab.
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
  yield BrowserTestUtils.removeTab(tab);

  // Restore the most recently closed item.
  extension.sendMessage("restore");
  restored = yield extension.awaitMessage("restored");

  is(restored.length, 1, "restore returned the expected number of sessions");
  tab = restored[0].tab;
  ok(tab, "restore returned a tab");
  checkLocalTab(tab, "about:robots");

  // Close the tab again.
  let realTab = TabManager.getTab(tab.id);
  yield BrowserTestUtils.removeTab(realTab);

  // Restore the tab using the sessionId.
  extension.sendMessage("check-sessions");
  recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  extension.sendMessage("restore", recentlyClosed[0].tab.sessionId);
  restored = yield extension.awaitMessage("restored");

  is(restored.length, 1, "restore returned the expected number of sessions");
  tab = restored[0].tab;
  ok(tab, "restore returned a tab");
  checkLocalTab(tab, "about:robots");

  // Close the tab again.
  realTab = TabManager.getTab(tab.id);
  yield BrowserTestUtils.removeTab(realTab);

  // Try to restore something with an invalid sessionId.
  extension.sendMessage("restore-reject");
  restored = yield extension.awaitMessage("restore-rejected");

  yield extension.unload();
});
