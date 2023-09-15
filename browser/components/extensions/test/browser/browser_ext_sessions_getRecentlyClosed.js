/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

requestLongerTimeout(2);

loadTestSubscript("head_sessions.js");

add_task(async function test_sessions_get_recently_closed() {
  async function openAndCloseWindow(url = "http://example.com", tabUrls) {
    let win = await BrowserTestUtils.openNewBrowserWindow();
    BrowserTestUtils.startLoadingURIString(win.gBrowser.selectedBrowser, url);
    await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    if (tabUrls) {
      for (let url of tabUrls) {
        await BrowserTestUtils.openNewForegroundTab(win.gBrowser, url);
      }
    }
    await BrowserTestUtils.closeWindow(win);
  }

  function background() {
    Promise.all([
      browser.sessions.getRecentlyClosed(),
      browser.tabs.query({ active: true, currentWindow: true }),
    ]).then(([recentlyClosed, tabs]) => {
      browser.test.sendMessage("initialData", {
        recentlyClosed,
        currentWindowId: tabs[0].windowId,
      });
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
  await openAndCloseWindow();

  await extension.startup();

  let { recentlyClosed, currentWindowId } = await extension.awaitMessage(
    "initialData"
  );
  recordInitialTimestamps(recentlyClosed.map(item => item.lastModified));

  await openAndCloseWindow();
  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  checkRecentlyClosed(
    recentlyClosed.filter(onlyNewItemsFilter),
    1,
    currentWindowId
  );

  await openAndCloseWindow("about:config", ["about:robots", "about:mozilla"]);
  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  // Check for multiple tabs in most recently closed window
  is(
    recentlyClosed[0].window.tabs.length,
    3,
    "most recently closed window has the expected number of tabs"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com"
  );
  BrowserTestUtils.removeTab(tab);

  tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com"
  );
  BrowserTestUtils.removeTab(tab);

  await openAndCloseWindow();
  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
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
  extension.sendMessage("check-sessions", { maxResults: 2 });
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  checkRecentlyClosed(
    recentlyClosed.filter(onlyNewItemsFilter),
    2,
    currentWindowId
  );

  await extension.unload();
});

add_task(async function test_sessions_get_recently_closed_navigated() {
  function background() {
    browser.sessions
      .getRecentlyClosed({ maxResults: 1 })
      .then(recentlyClosed => {
        let tab = recentlyClosed[0].window.tabs[0];
        browser.test.assertEq(
          "http://example.com/",
          tab.url,
          "Tab in closed window has the expected url."
        );
        browser.test.assertTrue(
          tab.title.includes("mochitest index"),
          "Tab in closed window has the expected title."
        );
        browser.test.notifyPass("getRecentlyClosed with navigation");
      });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions", "tabs"],
    },
    background,
  });

  // Test with a window with navigation history.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  for (let url of ["about:robots", "about:mozilla", "http://example.com/"]) {
    BrowserTestUtils.startLoadingURIString(win.gBrowser.selectedBrowser, url);
    await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  }

  await BrowserTestUtils.closeWindow(win);

  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
});

add_task(
  async function test_sessions_get_recently_closed_empty_history_in_closed_window() {
    function background() {
      browser.sessions
        .getRecentlyClosed({ maxResults: 1 })
        .then(recentlyClosed => {
          let win = recentlyClosed[0].window;
          browser.test.assertEq(
            3,
            win.tabs.length,
            "The closed window has 3 tabs."
          );
          browser.test.assertEq(
            "about:blank",
            win.tabs[0].url,
            "The first tab is about:blank."
          );
          browser.test.assertFalse(
            "url" in win.tabs[1],
            "The second tab with empty.xpi has no url field due to empty history."
          );
          browser.test.assertEq(
            "http://example.com/",
            win.tabs[2].url,
            "The third tab is example.com."
          );
          browser.test.notifyPass("getRecentlyClosed with empty history");
        });
    }

    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["sessions", "tabs"],
      },
      background,
    });

    // Test with a window with empty history.
    let xpi =
      "http://example.com/browser/browser/components/extensions/test/browser/empty.xpi";
    let newWin = await BrowserTestUtils.openNewBrowserWindow();
    await BrowserTestUtils.openNewForegroundTab({
      gBrowser: newWin.gBrowser,
      url: xpi,
      // A tab with broken xpi file doesn't finish loading.
      waitForLoad: false,
    });
    await BrowserTestUtils.openNewForegroundTab({
      gBrowser: newWin.gBrowser,
      url: "http://example.com/",
    });
    await BrowserTestUtils.closeWindow(newWin);

    await extension.startup();
    await extension.awaitFinish();
    await extension.unload();
  }
);
