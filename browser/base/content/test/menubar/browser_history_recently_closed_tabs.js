/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/**
 * This test verifies behavior from bug 1819675:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1819675
 *
 * The recently closed tabs menu item should be enabled when there are tabs
 * closed from any window that is in the same private/non-private bucket as
 * the current window.
 */

const openedTabs = [];

async function closeTab(tab) {
  const sessionStoreChanged = TestUtils.topicObserved(
    "sessionstore-closed-objects-changed"
  );
  BrowserTestUtils.removeTab(tab);
  await sessionStoreChanged;
  const idx = openedTabs.indexOf(tab);
  ok(idx >= 0, "Tab was found in the openedTabs array");
  openedTabs.splice(idx, 1);
}

async function openTab(win = window, url) {
  const tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, url);
  openedTabs.push(tab);
}

async function checkMenu(window, expected) {
  await SimpleTest.promiseFocus(window);
  const historyMenubarItem = window.document.getElementById("history-menu");
  const historyMenu = window.document.getElementById("historyMenuPopup");
  const recentlyClosedTabsItem = historyMenu.querySelector("#historyUndoMenu");

  const menuShown = BrowserTestUtils.waitForEvent(historyMenu, "popupshown");
  historyMenubarItem.openMenu(true);
  info("checkMenu:, waiting for menuShown");
  await menuShown;

  Assert.equal(
    recentlyClosedTabsItem.disabled,
    expected.menuItemDisabled,
    `Recently closed tabs menu item is ${
      expected.menuItemDisabled ? "disabled" : "not disabled"
    }`
  );
  const menuHidden = BrowserTestUtils.waitForEvent(historyMenu, "popuphidden");
  historyMenu.hidePopup();
  info("checkMenu:, waiting for menuHidden");
  await menuHidden;
  info("checkMenu:, menuHidden, returning");
}

async function resetHistory() {
  // Clear the lists of closed windows and tabs.
  let sessionStoreChanged;
  while (SessionStore.getClosedWindowCount() > 0) {
    sessionStoreChanged = TestUtils.topicObserved(
      "sessionstore-closed-objects-changed"
    );
    SessionStore.forgetClosedWindow(0);
    await sessionStoreChanged;
  }
  for (const win of BrowserWindowTracker.orderedWindows) {
    while (SessionStore.getClosedTabCountForWindow(win)) {
      sessionStoreChanged = TestUtils.topicObserved(
        "sessionstore-closed-objects-changed"
      );
      SessionStore.forgetClosedTab(win, 0);
      await sessionStoreChanged;
    }
    is(
      SessionStore.getClosedTabCountForWindow(win),
      0,
      "Expect 0 closed tabs for this window"
    );
  }
}

add_task(async function test_recently_closed_tabs_nonprivate() {
  await resetHistory();
  is(openedTabs.length, 0, "Got expected openedTabs length");

  const win1 = window;
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  await openTab(win1, "https://example.com");
  // we're going to close a tab and don't want to accidentally close the window when it has 0 tabs
  await openTab(win2, "about:about");
  await openTab(win2, "https://example.org");
  is(openedTabs.length, 3, "Got expected openedTabs length");

  info("Checking the menuitem is initially disabled in both windows");
  for (let win of [win1, win2]) {
    await checkMenu(win, {
      menuItemDisabled: true,
    });
  }

  await closeTab(win2.gBrowser.selectedTab);
  is(openedTabs.length, 2, "Got expected openedTabs length");
  is(
    SessionStore.getClosedTabCount(),
    1,
    "Expect closed tab count of 1 after closing a tab"
  );

  for (let win of [win1, win2]) {
    await checkMenu(win, {
      menuItemDisabled: false,
    });
  }

  // clean up
  info("clean up opened window");
  const sessionStoreChanged = TestUtils.topicObserved(
    "sessionstore-closed-objects-changed"
  );
  await BrowserTestUtils.closeWindow(win2);
  await sessionStoreChanged;

  info("starting tab cleanup");
  while (gBrowser.tabs.length > 1) {
    await closeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  }
  info("finished tab cleanup");
  openedTabs.length = 0;
});

add_task(async function test_recently_closed_tabs_nonprivate_pref_off() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.closedTabsFromAllWindows", false]],
  });
  await resetHistory();
  is(openedTabs.length, 0, "Got expected openedTabs length");

  const win1 = window;
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  await openTab(win1, "https://example.com");
  // we're going to close a tab and don't want to accidentally close the window when it has 0 tabs
  await openTab(win2, "about:about");
  await openTab(win2, "https://example.org");
  is(openedTabs.length, 3, "Got expected openedTabs length");

  info("Checking the menuitem is initially disabled in both windows");
  for (let win of [win1, win2]) {
    await checkMenu(win, {
      menuItemDisabled: true,
    });
  }
  is(win2, BrowserWindowTracker.getTopWindow(), "Check topWindow");

  await closeTab(win2.gBrowser.selectedTab);
  is(openedTabs.length, 2, "Got expected openedTabs length");
  is(
    SessionStore.getClosedTabCount(),
    1,
    "Expect closed tab count of 1 after closing a tab"
  );

  await checkMenu(win1, {
    menuItemDisabled: true,
  });
  await checkMenu(win2, {
    menuItemDisabled: false,
  });

  // clean up
  info("clean up opened window");
  const sessionStoreChanged = TestUtils.topicObserved(
    "sessionstore-closed-objects-changed"
  );
  await BrowserTestUtils.closeWindow(win2);
  await sessionStoreChanged;

  info("starting tab cleanup");
  while (gBrowser.tabs.length > 1) {
    await closeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  }
  info("finished tab cleanup");
  openedTabs.length = 0;
  SpecialPowers.popPrefEnv();
});

add_task(async function test_recently_closed_tabs_mixed_private() {
  await resetHistory();
  is(openedTabs.length, 0, "Got expected openedTabs length");
  is(
    SessionStore.getClosedTabCount(),
    0,
    "Expect closed tab count of 0 after reset"
  );

  await openTab(window, "about:robots");
  await openTab(window, "https://example.com");

  const privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await openTab(privateWin, "about:about");
  await openTab(privateWin, "https://example.org");
  is(openedTabs.length, 4, "Got expected openedTabs length");

  for (let win of [window, privateWin]) {
    await checkMenu(win, {
      menuItemDisabled: true,
    });
  }

  await closeTab(privateWin.gBrowser.selectedTab);
  is(openedTabs.length, 3, "Got expected openedTabs length");
  is(
    SessionStore.getClosedTabCount(privateWin),
    1,
    "Expect closed tab count of 1 for private windows"
  );
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Expect closed tab count of 0 for non-private windows"
  );

  // the menu should be enabled only for the private window
  await checkMenu(window, {
    menuItemDisabled: true,
  });
  await checkMenu(privateWin, {
    menuItemDisabled: false,
  });

  await resetHistory();
  is(
    SessionStore.getClosedTabCount(privateWin),
    0,
    "Expect 0 closed tab count after reset"
  );
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Expect 0 closed tab count after reset"
  );

  info("closing tab in non-private window");
  await closeTab(window.gBrowser.selectedTab);
  is(openedTabs.length, 2, "Got expected openedTabs length");

  // the menu should be enabled only for the non-private window
  await checkMenu(window, {
    menuItemDisabled: false,
  });
  await checkMenu(privateWin, {
    menuItemDisabled: true,
  });

  // clean up
  info("closing private window");
  await BrowserTestUtils.closeWindow(privateWin);
  await TestUtils.waitForTick();

  info("starting tab cleanup");
  while (gBrowser.tabs.length > 1) {
    await closeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  }
  info("finished tab cleanup");
  openedTabs.length = 0;
});

add_task(async function test_recently_closed_tabs_mixed_private_pref_off() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.closedTabsFromAllWindows", false]],
  });
  await resetHistory();
  is(openedTabs.length, 0, "Got expected openedTabs length");
  is(
    SessionStore.getClosedTabCount(),
    0,
    "Expect closed tab count of 0 after reset"
  );

  await openTab(window, "about:robots");
  await openTab(window, "https://example.com");

  const privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await openTab(privateWin, "about:about");
  await openTab(privateWin, "https://example.org");
  is(openedTabs.length, 4, "Got expected openedTabs length");

  for (let win of [window, privateWin]) {
    await checkMenu(win, {
      menuItemDisabled: true,
    });
  }

  await closeTab(privateWin.gBrowser.selectedTab);
  is(openedTabs.length, 3, "Got expected openedTabs length");
  is(
    SessionStore.getClosedTabCount(privateWin),
    1,
    "Expect closed tab count of 1 for private windows"
  );
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Expect closed tab count of 0 for non-private windows"
  );

  // the menu should be enabled only for the private window
  await checkMenu(window, {
    menuItemDisabled: true,
  });
  await checkMenu(privateWin, {
    menuItemDisabled: false,
  });

  await resetHistory();
  is(
    SessionStore.getClosedTabCount(privateWin),
    0,
    "Expect 0 closed tab count after reset"
  );
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Expect 0 closed tab count after reset"
  );

  info("closing tab in non-private window");
  await closeTab(window.gBrowser.selectedTab);
  is(openedTabs.length, 2, "Got expected openedTabs length");

  // the menu should be enabled only for the non-private window
  await checkMenu(window, {
    menuItemDisabled: false,
  });
  await checkMenu(privateWin, {
    menuItemDisabled: true,
  });

  // clean up
  info("closing private window");
  await BrowserTestUtils.closeWindow(privateWin);
  await TestUtils.waitForTick();

  info("starting tab cleanup");
  while (gBrowser.tabs.length > 1) {
    await closeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  }
  info("finished tab cleanup");
  openedTabs.length = 0;
  SpecialPowers.popPrefEnv();
});
