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

const { SessionStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SessionStoreTestUtils.sys.mjs"
);
const triggeringPrincipal_base64 = E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;
SessionStoreTestUtils.init(this, window);

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

function resetClosedTabsAndWindows() {
  // Clear the lists of closed windows and tabs.
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(SessionStore.getClosedWindowCount(), 0, "Expect 0 closed windows");
  for (const win of BrowserWindowTracker.orderedWindows) {
    is(
      SessionStore.getClosedTabCountForWindow(win),
      0,
      "Expect 0 closed tabs for this window"
    );
  }
}

add_task(async function test_recently_closed_tabs_nonprivate() {
  await resetClosedTabsAndWindows();

  const win1 = window;
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab(
    win1.gBrowser,
    "https://example.com"
  );
  // we're going to close a tab and don't want to accidentally close the window when it has 0 tabs
  await BrowserTestUtils.openNewForegroundTab(win2.gBrowser, "about:about");
  await BrowserTestUtils.openNewForegroundTab(
    win2.gBrowser,
    "https://example.org"
  );

  info("Checking the menuitem is initially disabled in both windows");
  for (let win of [win1, win2]) {
    await checkMenu(win, {
      menuItemDisabled: true,
    });
  }

  await SessionStoreTestUtils.closeTab(win2.gBrowser.selectedTab);
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
    await SessionStoreTestUtils.closeTab(
      gBrowser.tabs[gBrowser.tabs.length - 1]
    );
  }
  info("finished tab cleanup");
});

add_task(async function test_recently_closed_tabs_nonprivate_pref_off() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.closedTabsFromAllWindows", false]],
  });
  await resetClosedTabsAndWindows();

  const win1 = window;
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab(
    win1.gBrowser,
    "https://example.com"
  );
  // we're going to close a tab and don't want to accidentally close the window when it has 0 tabs
  await BrowserTestUtils.openNewForegroundTab(win2.gBrowser, "about:about");
  await BrowserTestUtils.openNewForegroundTab(
    win2.gBrowser,
    "https://example.org"
  );

  info("Checking the menuitem is initially disabled in both windows");
  for (let win of [win1, win2]) {
    await checkMenu(win, {
      menuItemDisabled: true,
    });
  }
  await SimpleTest.promiseFocus(win2);
  await SessionStoreTestUtils.closeTab(win2.gBrowser.selectedTab);
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
    await SessionStoreTestUtils.closeTab(
      gBrowser.tabs[gBrowser.tabs.length - 1]
    );
  }
  info("finished tab cleanup");
  SpecialPowers.popPrefEnv();
});

add_task(async function test_recently_closed_tabs_mixed_private() {
  await resetClosedTabsAndWindows();
  is(
    SessionStore.getClosedTabCount(),
    0,
    "Expect closed tab count of 0 after reset"
  );

  await BrowserTestUtils.openNewForegroundTab(window.gBrowser, "about:robots");
  await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "https://example.com"
  );

  const privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    "about:about"
  );
  await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    "https://example.org"
  );

  for (let win of [window, privateWin]) {
    await checkMenu(win, {
      menuItemDisabled: true,
    });
  }

  await SessionStoreTestUtils.closeTab(privateWin.gBrowser.selectedTab);
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

  await resetClosedTabsAndWindows();
  await SimpleTest.promiseFocus(window);

  info("closing tab in non-private window");
  await SessionStoreTestUtils.closeTab(window.gBrowser.selectedTab);
  is(
    SessionStore.getClosedTabCount(window),
    1,
    "Expect 1 closed tab count after closing the a tab in the non-private window"
  );

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
    await SessionStoreTestUtils.closeTab(
      gBrowser.tabs[gBrowser.tabs.length - 1]
    );
  }
  info("finished tab cleanup");
});

add_task(async function test_recently_closed_tabs_mixed_private_pref_off() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.closedTabsFromAllWindows", false]],
  });
  await resetClosedTabsAndWindows();

  await BrowserTestUtils.openNewForegroundTab(window.gBrowser, "about:robots");
  await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "https://example.com"
  );

  const privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    "about:about"
  );
  await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    "https://example.org"
  );

  for (let win of [window, privateWin]) {
    await checkMenu(win, {
      menuItemDisabled: true,
    });
  }

  await SimpleTest.promiseFocus(privateWin);
  await SessionStoreTestUtils.closeTab(privateWin.gBrowser.selectedTab);
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

  await resetClosedTabsAndWindows();
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
  await SimpleTest.promiseFocus(window);
  await SessionStoreTestUtils.closeTab(window.gBrowser.selectedTab);

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
    await SessionStoreTestUtils.closeTab(
      gBrowser.tabs[gBrowser.tabs.length - 1]
    );
  }
  info("finished tab cleanup");
  SpecialPowers.popPrefEnv();
});

add_task(async function test_recently_closed_tabs_closed_windows() {
  // prepare window state with closed tabs from closed windows
  await SpecialPowers.pushPrefEnv({
    set: [["sessionstore.closedTabsFromClosedWindows", true]],
  });
  const closedTabUrls = ["about:robots"];
  const closedWindowState = {
    tabs: [
      {
        entries: [{ url: "about:mozilla", triggeringPrincipal_base64 }],
      },
    ],
    _closedTabs: closedTabUrls.map(url => {
      return {
        state: {
          entries: [
            {
              url,
              triggeringPrincipal_base64,
            },
          ],
        },
      };
    }),
  };
  await SessionStoreTestUtils.promiseBrowserState({
    windows: [
      {
        tabs: [
          {
            entries: [{ url: "about:mozilla", triggeringPrincipal_base64 }],
          },
        ],
      },
    ],
    _closedWindows: [closedWindowState],
  });

  // verify the recently-closed-tabs menu item is enabled
  await checkMenu(window, {
    menuItemDisabled: false,
  });

  // flip the pref
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.closedTabsFromClosedWindows", false]],
  });

  // verify the recently-closed-tabs menu item is disabled
  await checkMenu(window, {
    menuItemDisabled: true,
  });

  SpecialPowers.popPrefEnv();
});
