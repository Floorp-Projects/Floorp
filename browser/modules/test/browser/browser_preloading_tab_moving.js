/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gOldCount = NewTabPagePreloading.MAX_COUNT;
registerCleanupFunction(() => {
  NewTabPagePreloading.MAX_COUNT = gOldCount;
});

async function openWinWithPreloadBrowser(options = {}) {
  let idleFinishedPromise = TestUtils.topicObserved("browser-idle-startup-tasks-finished", w => {
    return w != window;
  });
  let newWin = await BrowserTestUtils.openNewBrowserWindow(options);
  await idleFinishedPromise;
  await TestUtils.waitForCondition(() => newWin.gBrowser.preloadedBrowser);
  return newWin;
}

/**
 * Verify that moving a preloaded browser's content from one window to the next
 * works correctly.
 */
add_task(async function moving_works() {
  NewTabPagePreloading.MAX_COUNT = 1;

  NewTabPagePreloading.removePreloadedBrowser(window);

  NewTabPagePreloading.maybeCreatePreloadedBrowser(window);
  isnot(gBrowser.preloadedBrowser, null, "Should have preloaded browser");

  let oldKey = gBrowser.preloadedBrowser.permanentKey;

  let newWin = await openWinWithPreloadBrowser();
  is(gBrowser.preloadedBrowser, null, "Preloaded browser should be gone");
  isnot(newWin.gBrowser.preloadedBrowser, null, "Should have moved the preload browser");
  is(newWin.gBrowser.preloadedBrowser.permanentKey, oldKey, "Should have the same permanent key");
  let browser = newWin.gBrowser.preloadedBrowser;
  let tab = BrowserTestUtils.addTab(newWin.gBrowser, newWin.BROWSER_NEW_TAB_URL);
  is(tab.linkedBrowser, browser, "Preloaded browser is usable when opening a new tab.");
  await ContentTask.spawn(browser, null, function() {
    return ContentTaskUtils.waitForCondition(() => {
      return content.document.readyState == "complete";
    });
  });
  ok(true, "Successfully loaded the tab.");

  tab = browser = null;
  await BrowserTestUtils.closeWindow(newWin);

  tab = BrowserTestUtils.addTab(gBrowser, BROWSER_NEW_TAB_URL);
  await ContentTask.spawn(tab.linkedBrowser, null, function() {
    return ContentTaskUtils.waitForCondition(() => {
      return content.document.readyState == "complete";
    });
  });

  ok(true, "Managed to open a tab in the original window still.");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function moving_shouldnt_move_across_private_state() {
  NewTabPagePreloading.MAX_COUNT = 1;

  NewTabPagePreloading.removePreloadedBrowser(window);

  NewTabPagePreloading.maybeCreatePreloadedBrowser(window);
  isnot(gBrowser.preloadedBrowser, null, "Should have preloaded browser");

  let oldKey = gBrowser.preloadedBrowser.permanentKey;
  let newWin = await openWinWithPreloadBrowser({private: true});

  isnot(gBrowser.preloadedBrowser, null, "Preloaded browser in original window should persist");
  isnot(newWin.gBrowser.preloadedBrowser, null, "Should have created another preload browser");
  isnot(newWin.gBrowser.preloadedBrowser.permanentKey, oldKey, "Should not have the same permanent key");
  let browser = newWin.gBrowser.preloadedBrowser;
  let tab = BrowserTestUtils.addTab(newWin.gBrowser, newWin.BROWSER_NEW_TAB_URL);
  is(tab.linkedBrowser, browser, "Preloaded browser is usable when opening a new tab.");
  await ContentTask.spawn(browser, null, function() {
    return ContentTaskUtils.waitForCondition(() => {
      return content.document.readyState == "complete";
    });
  });
  ok(true, "Successfully loaded the tab.");

  tab = browser = null;
  await BrowserTestUtils.closeWindow(newWin);
});

