/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

SimpleTest.requestCompleteLog();

let initialTimestamps = [];

function onlyNewItemsFilter(item) {
  return !initialTimestamps.includes(item.lastModified);
}

function checkWindow(window) {
  for (let prop of ["focused", "incognito", "alwaysOnTop"]) {
    is(window[prop], false, `closed window has the expected value for ${prop}`);
  }
  for (let prop of ["state", "type"]) {
    is(window[prop], "normal", `closed window has the expected value for ${prop}`);
  }
}

function checkTab(tab, windowId, incognito) {
  for (let prop of ["selected", "highlighted", "active", "pinned"]) {
    is(tab[prop], false, `closed tab has the expected value for ${prop}`);
  }
  is(tab.windowId, windowId, "closed tab has the expected value for windowId");
  is(tab.incognito, incognito, "closed tab has the expected value for incognito");
}

function checkRecentlyClosed(recentlyClosed, expectedCount, windowId, incognito = false) {
  let sessionIds = new Set();
  is(recentlyClosed.length, expectedCount, "the expected number of closed tabs/windows was found");
  for (let item of recentlyClosed) {
    if (item.window) {
      sessionIds.add(item.window.sessionId);
      checkWindow(item.window);
    } else if (item.tab) {
      sessionIds.add(item.tab.sessionId);
      checkTab(item.tab, windowId, incognito);
    }
  }
  is(sessionIds.size, expectedCount, "each item has a unique sessionId");
}

add_task(function* test_sessions_get_recently_closed() {
  function* openAndCloseWindow(url = "http://example.com", tabUrls) {
    let win = yield BrowserTestUtils.openNewBrowserWindow();
    yield BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, url);
    yield BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    if (tabUrls) {
      for (let url of tabUrls) {
        yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, url);
      }
    }
    yield BrowserTestUtils.closeWindow(win);
  }

  function background() {
    Promise.all([
      browser.sessions.getRecentlyClosed(),
      browser.tabs.query({active: true, currentWindow: true}),
    ]).then(([recentlyClosed, tabs]) => {
      browser.test.sendMessage("initialData", {recentlyClosed, currentWindowId: tabs[0].windowId});
    });

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

  // Open and close a window that will be ignored, to prove that we are removing previous entries
  yield openAndCloseWindow();

  yield extension.startup();

  let {recentlyClosed, currentWindowId} = yield extension.awaitMessage("initialData");
  initialTimestamps = recentlyClosed.map(item => item.lastModified);

  yield openAndCloseWindow();
  extension.sendMessage("check-sessions");
  recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  checkRecentlyClosed(recentlyClosed.filter(onlyNewItemsFilter), 1, currentWindowId);

  yield openAndCloseWindow("about:config", ["about:robots", "about:mozilla"]);
  extension.sendMessage("check-sessions");
  recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  // Check for multiple tabs in most recently closed window
  is(recentlyClosed[0].window.tabs.length, 3, "most recently closed window has the expected number of tabs");

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");
  yield BrowserTestUtils.removeTab(tab);

  tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");
  yield BrowserTestUtils.removeTab(tab);

  yield openAndCloseWindow();
  extension.sendMessage("check-sessions");
  recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  let finalResult = recentlyClosed.filter(onlyNewItemsFilter);
  checkRecentlyClosed(finalResult, 5, currentWindowId);

  isnot(finalResult[0].window, undefined, "first item is a window");
  is(finalResult[0].tab, undefined, "first item is not a tab");
  isnot(finalResult[1].tab, undefined, "second item is a tab");
  is(finalResult[1].window, undefined, "second item is not a window");
  isnot(finalResult[2].tab, undefined, "third item is a tab");
  is(finalResult[2].window, undefined, "third item is not a window");
  isnot(finalResult[3].window, undefined, "fourth item is a window");
  is(finalResult[3].tab, undefined, "fourth item is not a tab");
  isnot(finalResult[4].window, undefined, "fifth item is a window");
  is(finalResult[4].tab, undefined, "fifth item is not a tab");

  // test with filter
  extension.sendMessage("check-sessions", {maxResults: 2});
  recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  checkRecentlyClosed(recentlyClosed.filter(onlyNewItemsFilter), 2, currentWindowId);

  yield extension.unload();
});

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

  let {Management: {global: {WindowManager}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let privateWinId = WindowManager.getId(privateWin);

  yield extension.startup();

  extension.sendMessage("check-sessions");
  let recentlyClosed = yield extension.awaitMessage("recentlyClosed");
  initialTimestamps = recentlyClosed.map(item => item.lastModified);

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
