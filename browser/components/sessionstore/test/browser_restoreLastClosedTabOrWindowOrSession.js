/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { _LastSession, _lastClosedActions } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/SessionStore.sys.mjs"
);

async function testLastClosedActionsEntries() {
  SessionStore.resetLastClosedActions();

  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  BrowserTestUtils.loadURIString(
    win2.gBrowser.selectedBrowser,
    "https://www.mozilla.org/"
  );

  await BrowserTestUtils.browserLoaded(win2.gBrowser.selectedBrowser);
  await openAndCloseTab(win2, "https://example.org/");

  Assert.ok(
    SessionStore.lastClosedActions.length == 1,
    `1 closed action has been recorded`
  );

  await BrowserTestUtils.closeWindow(win2);

  Assert.ok(
    SessionStore.lastClosedActions.length == 2,
    `2 closed actions have been recorded`
  );
}

/**
 * Tests that if the user invokes restoreLastClosedTabOrWindowOrSession it will result in either the last session will be restored, if possible,
 * the last tab (or multiple selected tabs) that was closed is reopened, or the last window that is closed is reopened.
 */
add_task(async function test_undo_last_action() {
  // forgetClosedTabs(window);
  // needed for verify tests so that forgetting tabs isn't recorded
  SessionStore.resetLastClosedActions();

  registerCleanupFunction(() => {
    forgetClosedTabs(window);
  });

  const state = {
    windows: [
      {
        tabs: [
          {
            entries: [
              {
                title: "example.org",
                url: "https://example.org/",
                triggeringPrincipal_base64,
              },
            ],
          },
          {
            entries: [
              {
                title: "example.com",
                url: "https://example.com/",
                triggeringPrincipal_base64,
              },
            ],
          },
          {
            entries: [
              {
                title: "mozilla",
                url: "https://www.mozilla.org/",
                triggeringPrincipal_base64,
              },
            ],
          },
        ],
        selected: 3,
      },
    ],
  };

  _LastSession.setState(state);

  let sessionRestored = promiseSessionStoreLoads(3 /* total restored tabs */);
  restoreLastClosedTabOrWindowOrSession();
  await sessionRestored;
  Assert.ok(
    window.gBrowser.tabs.length == 3,
    "Window has three tabs after session is restored"
  );

  // open and close a window, then reopen it
  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  Assert.ok(win2.gBrowser.tabs.length == 1, "Second window has one open tab");
  BrowserTestUtils.loadURIString(
    win2.gBrowser.selectedBrowser,
    "https://example.com/"
  );
  await BrowserTestUtils.browserLoaded(win2.gBrowser.selectedBrowser);
  await BrowserTestUtils.closeWindow(win2);
  let restoredWinPromise = BrowserTestUtils.waitForNewWindow({
    url: "https://example.com/",
  });
  restoreLastClosedTabOrWindowOrSession();
  let restoredWin = await restoredWinPromise;
  Assert.ok(
    restoredWin.gBrowser.tabs.length == 1,
    "First tab in the second window has been reopened"
  );
  await BrowserTestUtils.closeWindow(restoredWin);
  SessionStore.forgetClosedWindow(restoredWin.index);
  restoreLastClosedTabOrWindowOrSession();

  // close one tab and reopen it
  BrowserTestUtils.removeTab(window.gBrowser.tabs[2]);
  Assert.ok(window.gBrowser.tabs.length == 2, "Window has two open tabs");
  restoreLastClosedTabOrWindowOrSession();
  Assert.ok(window.gBrowser.tabs.length == 3, "Window now has three open tabs");

  // select 2 tabs and close both via the 'close 2 tabs' context menu option
  let tab2 = window.gBrowser.tabs[1];
  let tab3 = window.gBrowser.tabs[2];
  await triggerClickOn(tab2, { ctrlKey: true });
  Assert.equal(tab2.multiselected, true);
  Assert.equal(tab3.multiselected, true);

  let menu = await openTabMenuFor(tab3);
  let menuItemCloseTab = document.getElementById("context_closeTab");
  let tab2Closing = BrowserTestUtils.waitForTabClosing(tab2);
  let tab3Closing = BrowserTestUtils.waitForTabClosing(tab3);
  menu.activateItem(menuItemCloseTab);
  await tab2Closing;
  await tab3Closing;
  Assert.equal(window.gBrowser.tabs[0].selected, true);
  await TestUtils.waitForCondition(() => window.gBrowser.tabs.length == 1);
  Assert.ok(
    window.gBrowser.tabs.length == 1,
    "Window now has one open tab after closing two multi-selected tabs"
  );

  // ensure both tabs are reopened with a single click
  restoreLastClosedTabOrWindowOrSession();
  Assert.ok(
    window.gBrowser.tabs.length == 3,
    "Window now has three open tabs after reopening closed tabs"
  );

  // close one tab and forget it - it should not be reopened
  BrowserTestUtils.removeTab(window.gBrowser.tabs[2]);
  Assert.ok(window.gBrowser.tabs.length == 2, "Window has two open tabs");
  SessionStore.forgetClosedTab(window, 0);
  restoreLastClosedTabOrWindowOrSession();
  Assert.ok(window.gBrowser.tabs.length == 2, "Window still has two open tabs");

  gBrowser.removeAllTabsBut(gBrowser.tabs[0]);
});

add_task(async function test_forget_closed_window() {
  await testLastClosedActionsEntries();
  SessionStore.forgetClosedWindow();

  // both the forgotten window and its closed tab has been removed from the list
  Assert.ok(
    !SessionStore.lastClosedActions.length,
    `0 closed actions have been recorded`
  );
});

add_task(async function test_user_clears_history() {
  await testLastClosedActionsEntries();
  Services.obs.notifyObservers(null, "browser:purge-session-history");

  // both the forgotten window and its closed tab has been removed from the list
  Assert.ok(
    !SessionStore.lastClosedActions.length,
    `0 closed actions have been recorded`
  );
});
