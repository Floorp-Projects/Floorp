/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests search mode and session store.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  TabStateFlusher: "resource:///modules/sessionstore/TabStateFlusher.jsm",
});

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });
});

const SEARCH_STRING = "test browser_sessionStore.js";
const URL = "http://example.com/";

// A URL in gInitialPages.  We test this separately since SessionStore sometimes
// takes different paths for these URLs.
const INITIAL_URL = "about:newtab";

// The following tasks make sure non-null search mode is restored.

add_task(async function initialPageOnRestore() {
  await doTest({
    urls: [INITIAL_URL],
    searchModeTabIndex: 0,
    exitSearchMode: false,
    switchTabsAfterEnteringSearchMode: false,
  });
});

add_task(async function switchToInitialPage() {
  await doTest({
    urls: [URL, INITIAL_URL],
    searchModeTabIndex: 1,
    exitSearchMode: false,
    switchTabsAfterEnteringSearchMode: true,
  });
});

add_task(async function nonInitialPageOnRestore() {
  await doTest({
    urls: [URL],
    searchModeTabIndex: 0,
    exitSearchMode: false,
    switchTabsAfterEnteringSearchMode: false,
  });
});

add_task(async function switchToNonInitialPage() {
  await doTest({
    urls: [INITIAL_URL, URL],
    searchModeTabIndex: 1,
    exitSearchMode: false,
    switchTabsAfterEnteringSearchMode: true,
  });
});

// The following tasks enter and then exit search mode to make sure that no
// search mode is restored.

add_task(async function initialPageOnRestore_exit() {
  await doTest({
    urls: [INITIAL_URL],
    searchModeTabIndex: 0,
    exitSearchMode: true,
    switchTabsAfterEnteringSearchMode: false,
  });
});

add_task(async function switchToInitialPage_exit() {
  await doTest({
    urls: [URL, INITIAL_URL],
    searchModeTabIndex: 1,
    exitSearchMode: true,
    switchTabsAfterEnteringSearchMode: true,
  });
});

add_task(async function nonInitialPageOnRestore_exit() {
  await doTest({
    urls: [URL],
    searchModeTabIndex: 0,
    exitSearchMode: true,
    switchTabsAfterEnteringSearchMode: false,
  });
});

add_task(async function switchToNonInitialPage_exit() {
  await doTest({
    urls: [INITIAL_URL, URL],
    searchModeTabIndex: 1,
    exitSearchMode: true,
    switchTabsAfterEnteringSearchMode: true,
  });
});

/**
 * The main test function.  Opens some URLs in a new window, enters search mode
 * in one of the tabs, closes the window, restores it, and makes sure that
 * search mode is restored properly.
 *
 * @param {array} urls
 *   Array of string URLs to open.
 * @param {number} searchModeTabIndex
 *   The index of the tab in which to enter search mode.
 * @param {boolean} exitSearchMode
 *   If true, search mode will be immediately exited after entering it.  Use
 *   this to make sure search mode is *not* restored after it's exited.
 * @param {boolean} switchTabsAfterEnteringSearchMode
 *   If true, we'll switch to a tab other than the one that search mode was
 *   entered in before closing the window.  `urls` should contain more than one
 *   URL in this case.
 */
async function doTest({
  urls,
  searchModeTabIndex,
  exitSearchMode,
  switchTabsAfterEnteringSearchMode,
}) {
  let searchModeURL = urls[searchModeTabIndex];
  let otherTabIndex = (searchModeTabIndex + 1) % urls.length;
  let otherURL = urls[otherTabIndex];

  await withNewWindow(urls, async win => {
    if (win.gBrowser.selectedTab != win.gBrowser.tabs[searchModeTabIndex]) {
      await BrowserTestUtils.switchTab(
        win.gBrowser,
        win.gBrowser.tabs[searchModeTabIndex]
      );
    }

    Assert.equal(
      win.gBrowser.currentURI.spec,
      searchModeURL,
      `Sanity check: Tab at index ${searchModeTabIndex} is correct`
    );
    Assert.equal(
      searchModeURL == INITIAL_URL,
      win.gInitialPages.includes(win.gBrowser.currentURI.spec),
      `Sanity check: ${searchModeURL} is or is not in gInitialPages as expected`
    );

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: SEARCH_STRING,
      fireInputEvent: true,
    });
    await UrlbarTestUtils.enterSearchMode(win, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });

    if (exitSearchMode) {
      await UrlbarTestUtils.exitSearchMode(win);
    }

    // Make sure session store is updated.
    await TabStateFlusher.flush(win.gBrowser.selectedBrowser);

    if (switchTabsAfterEnteringSearchMode) {
      await BrowserTestUtils.switchTab(
        win.gBrowser,
        win.gBrowser.tabs[otherTabIndex]
      );
    }
  });

  let restoredURL = switchTabsAfterEnteringSearchMode
    ? otherURL
    : searchModeURL;

  let win = await restoreWindow(restoredURL);

  Assert.equal(
    win.gBrowser.currentURI.spec,
    restoredURL,
    "Sanity check: Initially selected tab in restored window is correct"
  );

  if (switchTabsAfterEnteringSearchMode) {
    // Switch back to the tab with search mode.
    await BrowserTestUtils.switchTab(
      win.gBrowser,
      win.gBrowser.tabs[searchModeTabIndex]
    );
  }

  if (exitSearchMode) {
    // If we exited search mode, it should be null.
    await new Promise(r => win.setTimeout(r, 500));
    await UrlbarTestUtils.assertSearchMode(win, null);
  } else {
    // If we didn't exit search mode, it should be restored.
    await TestUtils.waitForCondition(
      () => win.gURLBar.searchMode,
      "Waiting for search mode to be restored"
    );
    await UrlbarTestUtils.assertSearchMode(win, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      entry: "oneoff",
    });
    Assert.equal(
      win.gURLBar.value,
      SEARCH_STRING,
      "Search string should be restored"
    );
  }

  await BrowserTestUtils.closeWindow(win);
}

/**
 * Opens a new browser window with the given URLs, calls a callback, and then
 * closes the window.
 *
 * @param {array} urls
 *   Array of string URLs to open.
 * @param {function} callback
 *   The callback.
 */
async function withNewWindow(urls, callback) {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  for (let url of urls) {
    await BrowserTestUtils.openNewForegroundTab({
      url,
      gBrowser: win.gBrowser,
      waitForLoad: url != "about:newtab",
    });
    if (url == "about:newtab") {
      await TestUtils.waitForCondition(
        () => win.gBrowser.currentURI.spec == "about:newtab",
        "Waiting for about:newtab"
      );
    }
  }
  BrowserTestUtils.removeTab(win.gBrowser.tabs[0]);
  await callback(win);
  await BrowserTestUtils.closeWindow(win);
}

/**
 * Uses SessionStore to reopen the last closed window.
 *
 * @param {string} expectedRestoredURL
 *   The URL you expect will be restored in the selected browser.
 */
async function restoreWindow(expectedRestoredURL) {
  let winPromise = BrowserTestUtils.waitForNewWindow();
  let win = SessionStore.undoCloseWindow(0);
  await winPromise;
  await TestUtils.waitForCondition(
    () => win.gBrowser.currentURI.spec == expectedRestoredURL,
    "Waiting for restored selected browser to have expected URI"
  );
  return win;
}
