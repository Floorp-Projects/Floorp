/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

SimpleTest.requestCompleteLog();

XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
                                  "resource:///modules/sessionstore/SessionStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TabStateFlusher",
                                  "resource:///modules/sessionstore/TabStateFlusher.jsm");

add_task(function* test_sessions_restore() {
  function background() {
    let notificationCount = 0;
    browser.sessions.onChanged.addListener(() => {
      notificationCount++;
      browser.test.sendMessage("notificationCount", notificationCount);
    });
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
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions", "tabs"],
    },
    background,
  });

  function* assertNotificationCount(expected) {
    let notificationCount = yield extension.awaitMessage("notificationCount");
    is(notificationCount, expected, "the expected number of notifications was fired");
  }

  yield extension.startup();

  let {Management: {global: {windowTracker, tabTracker}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  function checkLocalTab(tab, expectedUrl) {
    let realTab = tabTracker.getTab(tab.id);
    let tabState = JSON.parse(SessionStore.getTabState(realTab));
    is(tabState.entries[0].url, expectedUrl, "restored tab has the expected url");
  }

  yield extension.awaitMessage("ready");

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, "about:config");
  yield BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  for (let url of ["about:robots", "about:mozilla"]) {
    yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, url);
  }
  yield BrowserTestUtils.closeWindow(win);
  yield assertNotificationCount(1);

  extension.sendMessage("check-sessions");
  let recentlyClosed = yield extension.awaitMessage("recentlyClosed");

  // Check that our expected window is the most recently closed.
  is(recentlyClosed[0].window.tabs.length, 3, "most recently closed window has the expected number of tabs");

  // Restore the window.
  extension.sendMessage("restore");
  yield assertNotificationCount(2);
  let restored = yield extension.awaitMessage("restored");

  is(restored.window.tabs.length, 3, "restore returned a window with the expected number of tabs");
  checkLocalTab(restored.window.tabs[0], "about:config");
  checkLocalTab(restored.window.tabs[1], "about:robots");
  checkLocalTab(restored.window.tabs[2], "about:mozilla");

  // Close the window again.
  let window = windowTracker.getWindow(restored.window.id);
  yield BrowserTestUtils.closeWindow(window);
  yield assertNotificationCount(3);

  // Restore the window using the sessionId.
  extension.sendMessage("check-sessions");
  recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  extension.sendMessage("restore", recentlyClosed[0].window.sessionId);
  yield assertNotificationCount(4);
  restored = yield extension.awaitMessage("restored");

  is(restored.window.tabs.length, 3, "restore returned a window with the expected number of tabs");
  checkLocalTab(restored.window.tabs[0], "about:config");
  checkLocalTab(restored.window.tabs[1], "about:robots");
  checkLocalTab(restored.window.tabs[2], "about:mozilla");

  // Close the window again.
  window = windowTracker.getWindow(restored.window.id);
  yield BrowserTestUtils.closeWindow(window);
  // notificationCount = yield extension.awaitMessage("notificationCount");
  yield assertNotificationCount(5);

  // Open and close a tab.
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
  yield TabStateFlusher.flush(tab.linkedBrowser);
  yield BrowserTestUtils.removeTab(tab);
  yield assertNotificationCount(6);

  // Restore the most recently closed item.
  extension.sendMessage("restore");
  yield assertNotificationCount(7);
  restored = yield extension.awaitMessage("restored");

  tab = restored.tab;
  ok(tab, "restore returned a tab");
  checkLocalTab(tab, "about:robots");

  // Close the tab again.
  let realTab = tabTracker.getTab(tab.id);
  yield BrowserTestUtils.removeTab(realTab);
  yield assertNotificationCount(8);

  // Restore the tab using the sessionId.
  extension.sendMessage("check-sessions");
  recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  extension.sendMessage("restore", recentlyClosed[0].tab.sessionId);
  yield assertNotificationCount(9);
  restored = yield extension.awaitMessage("restored");

  tab = restored.tab;
  ok(tab, "restore returned a tab");
  checkLocalTab(tab, "about:robots");

  // Close the tab again.
  realTab = tabTracker.getTab(tab.id);
  yield BrowserTestUtils.removeTab(realTab);
  yield assertNotificationCount(10);

  // Try to restore something with an invalid sessionId.
  extension.sendMessage("restore-reject");
  restored = yield extension.awaitMessage("restore-rejected");

  yield extension.unload();
});
