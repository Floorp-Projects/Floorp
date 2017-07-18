/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function expectedTabInfo(tab, window) {
  let browser = tab.linkedBrowser;
  return {
    url: browser.currentURI.spec,
    title: browser.contentTitle,
    favIconUrl: window.gBrowser.getIcon(tab),
    // 'selected' is marked as unsupported in schema, so we've removed it.
    // For more details, see bug 1337509
    selected: undefined,
  };
}

function checkTabInfo(expected, actual) {
  for (let prop in expected) {
    is(actual[prop], expected[prop], `Expected value found for ${prop} of tab object.`);
  }
}

add_task(async function test_sessions_get_recently_closed_tabs() {
  async function background() {
    browser.test.onMessage.addListener(async msg => {
      if (msg == "check-sessions") {
        let recentlyClosed = await browser.sessions.getRecentlyClosed();
        browser.test.sendMessage("recentlyClosed", recentlyClosed);
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions", "tabs"],
    },
    background,
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, "about:mozilla");
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  let expectedTabs = [];
  let tab = win.gBrowser.selectedTab;
  expectedTabs.push(expectedTabInfo(tab, win));
  let lastAccessedTimes = new Map();
  lastAccessedTimes.set("about:mozilla", tab.lastAccessed);

  for (let url of ["about:robots", "about:buildconfig"]) {
    tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, url);
    expectedTabs.push(expectedTabInfo(tab, win));
    lastAccessedTimes.set(url, tab.lastAccessed);
  }

  await extension.startup();

  // Test with a closed tab.
  await BrowserTestUtils.removeTab(tab);

  extension.sendMessage("check-sessions");
  let recentlyClosed = await extension.awaitMessage("recentlyClosed");
  let tabInfo = recentlyClosed[0].tab;
  let expectedTab = expectedTabs.pop();
  checkTabInfo(expectedTab, tabInfo);
  ok(tabInfo.lastAccessed > lastAccessedTimes.get(tabInfo.url),
     "lastAccessed has been updated");

  // Test with a closed window containing tabs.
  await BrowserTestUtils.closeWindow(win);

  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  let tabInfos = recentlyClosed[0].window.tabs;
  is(tabInfos.length, 2, "Expected number of tabs in closed window.");
  for (let x = 0; x < tabInfos.length; x++) {
    checkTabInfo(expectedTabs[x], tabInfos[x]);
    ok(tabInfos[x].lastAccessed > lastAccessedTimes.get(tabInfos[x].url),
       "lastAccessed has been updated");
  }

  await extension.unload();

  // Test without tabs permission.
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions"],
    },
    background,
  });

  await extension.startup();

  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  tabInfos = recentlyClosed[0].window.tabs;
  is(tabInfos.length, 2, "Expected number of tabs in closed window.");
  for (let tabInfo of tabInfos) {
    for (let prop in expectedTabs[0]) {
      is(undefined,
         tabInfo[prop],
         `${prop} of tab object is undefined without tabs permission.`);
    }
  }

  await extension.unload();
});
