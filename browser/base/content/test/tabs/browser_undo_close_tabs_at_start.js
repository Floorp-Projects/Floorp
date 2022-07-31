/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_WARN_ON_CLOSE = "browser.tabs.warnOnCloseOtherTabs";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_WARN_ON_CLOSE, false]],
  });
});

add_task(async function replaceEmptyTabs() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const tabbrowser = win.gBrowser;
  ok(
    tabbrowser.tabs.length == 1 && tabbrowser.tabs[0].isEmpty,
    "One blank tab should be opened."
  );

  let blankTab = tabbrowser.tabs[0];
  await BrowserTestUtils.openNewForegroundTab(
    tabbrowser,
    "https://example.com/1"
  );
  await BrowserTestUtils.openNewForegroundTab(
    tabbrowser,
    "https://example.com/2"
  );
  await BrowserTestUtils.openNewForegroundTab(
    tabbrowser,
    "https://example.com/3"
  );

  is(tabbrowser.tabs.length, 4, "There should be 4 tabs opened.");

  tabbrowser.removeAllTabsBut(blankTab);

  await TestUtils.waitForCondition(
    () =>
      SessionStore.getLastClosedTabCount(win) == 3 &&
      tabbrowser.tabs.length == 1,
    "wait for the tabs to close in SessionStore"
  );
  is(
    SessionStore.getLastClosedTabCount(win),
    3,
    "SessionStore should know how many tabs were just closed"
  );

  is(tabbrowser.selectedTab, blankTab, "The blank tab should be selected.");

  win.undoCloseTab();

  await TestUtils.waitForCondition(
    () => tabbrowser.tabs.length == 3,
    "wait for the tabs to reopen"
  );

  is(
    SessionStore.getLastClosedTabCount(win),
    SessionStore.getClosedTabCount(win) ? 1 : 0,
    "LastClosedTabCount should be reset"
  );

  ok(
    !tabbrowser.tabs.includes(blankTab),
    "The blank tab should have been replaced."
  );

  // We can't (at the time of writing) check tab order.

  // Cleanup
  await BrowserTestUtils.closeWindow(win);
});
