/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function expectedTabInfo(tab, window) {
  let browser = tab.linkedBrowser;
  return {
    url: browser.currentURI.spec,
    title: browser.contentTitle,
    favIconUrl: window.gBrowser.getIcon(tab) || undefined,
    // 'selected' is marked as unsupported in schema, so we've removed it.
    // For more details, see bug 1337509
    selected: undefined,
  };
}

function checkTabInfo(expected, actual) {
  for (let prop in expected) {
    is(
      actual[prop],
      expected[prop],
      `Expected value found for ${prop} of tab object.`
    );
  }
}

add_task(async function test_sessions_get_recently_closed_tabs() {
  // Below, the test makes assumptions about the last accessed time of tabs that are
  // not true is we execute fast and reduce the timer precision enough
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.reduceTimerPrecision", false],
      ["browser.navigation.requireUserInteraction", false],
    ],
  });

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
  let tabBrowser = win.gBrowser.selectedBrowser;
  for (let url of ["about:robots", "about:mozilla", "about:config"]) {
    BrowserTestUtils.loadURI(tabBrowser, url);
    await BrowserTestUtils.browserLoaded(tabBrowser, false, url);
  }

  // Ensure that getRecentlyClosed returns correct results after the back
  // button has been used.
  let goBackPromise = BrowserTestUtils.waitForLocationChange(
    win.gBrowser,
    "about:mozilla"
  );
  tabBrowser.goBack();
  await goBackPromise;

  let expectedTabs = [];
  let tab = win.gBrowser.selectedTab;
  // Because there is debounce logic in ContentLinkHandler.jsm to reduce the
  // favicon loads, we have to wait some time before checking that icon was
  // stored properly. If that page doesn't have favicon links, let it timeout.
  try {
    await BrowserTestUtils.waitForCondition(
      () => {
        return gBrowser.getIcon(tab) != null;
      },
      "wait for favicon load to finish",
      100,
      5
    );
  } catch (e) {
    // This page doesn't have any favicon link, just continue.
  }
  expectedTabs.push(expectedTabInfo(tab, win));
  let lastAccessedTimes = new Map();
  lastAccessedTimes.set("about:mozilla", tab.lastAccessed);

  for (let url of ["about:robots", "about:buildconfig"]) {
    tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, url);
    try {
      await BrowserTestUtils.waitForCondition(
        () => {
          return gBrowser.getIcon(tab) != null;
        },
        "wait for favicon load to finish",
        100,
        5
      );
    } catch (e) {
      // This page doesn't have any favicon link, just continue.
    }
    expectedTabs.push(expectedTabInfo(tab, win));
    lastAccessedTimes.set(url, tab.lastAccessed);
  }

  await extension.startup();

  let sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  // Test with a closed tab.
  BrowserTestUtils.removeTab(tab);
  await sessionUpdatePromise;

  extension.sendMessage("check-sessions");
  let recentlyClosed = await extension.awaitMessage("recentlyClosed");
  let tabInfo = recentlyClosed[0].tab;
  let expectedTab = expectedTabs.pop();
  checkTabInfo(expectedTab, tabInfo);
  ok(
    tabInfo.lastAccessed > lastAccessedTimes.get(tabInfo.url),
    "lastAccessed has been updated"
  );

  // Test with a closed window containing tabs.
  await BrowserTestUtils.closeWindow(win);

  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  let tabInfos = recentlyClosed[0].window.tabs;
  is(tabInfos.length, 2, "Expected number of tabs in closed window.");
  for (let x = 0; x < tabInfos.length; x++) {
    checkTabInfo(expectedTabs[x], tabInfos[x]);
    ok(
      tabInfos[x].lastAccessed > lastAccessedTimes.get(tabInfos[x].url),
      "lastAccessed has been updated"
    );
  }

  await extension.unload();

  // Test without tabs and host permissions.
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
      is(
        undefined,
        tabInfo[prop],
        `${prop} of tab object is undefined without tabs permission.`
      );
    }
  }

  await extension.unload();

  // Test with host permission.
  win = await BrowserTestUtils.openNewBrowserWindow();
  tabBrowser = win.gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(tabBrowser, "http://example.com/testpage");
  await BrowserTestUtils.browserLoaded(
    tabBrowser,
    false,
    "http://example.com/testpage"
  );
  tab = win.gBrowser.getTabForBrowser(tabBrowser);
  try {
    await BrowserTestUtils.waitForCondition(
      () => {
        return gBrowser.getIcon(tab) != null;
      },
      "wait for favicon load to finish",
      100,
      5
    );
  } catch (e) {
    // This page doesn't have any favicon link, just continue.
  }
  expectedTab = expectedTabInfo(tab, win);
  await BrowserTestUtils.closeWindow(win);

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions", "http://example.com/*"],
    },
    background,
  });
  await extension.startup();

  extension.sendMessage("check-sessions");
  recentlyClosed = await extension.awaitMessage("recentlyClosed");
  tabInfo = recentlyClosed[0].window.tabs[0];
  checkTabInfo(expectedTab, tabInfo);

  await extension.unload();
});
