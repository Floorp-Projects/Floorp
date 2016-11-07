/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* globals recordInitialTimestamps, onlyNewItemsFilter, checkRecentlyClosed */

SimpleTest.requestCompleteLog();

Services.scriptloader.loadSubScript(new URL("head_sessions.js", gTestPath).href,
                                    this);

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
  recordInitialTimestamps(recentlyClosed.map(item => item.lastModified));

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
