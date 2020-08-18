/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that search mode is stored per tab and restored when switching tabs.
 */

"use strict";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });
});

add_task(async function switchTabs() {
  // Open three tabs.  We'll enter search mode in tabs 0 and 2.
  let tabs = [];
  for (let i = 0; i < 3; i++) {
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: "http://example.com/" + i,
    });
    tabs.push(tab);
  }

  // Switch to tab 0.
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);

  // Do a search and enter search mode.  Pass fireInputEvent so that
  // userTypedValue is set and restored when we switch back to this tab.  This
  // isn't really necessary but it simulates the user's typing, and it also
  // means that we'll start a search when we switch back to this tab.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.enterSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  // Switch to tab 1.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 0.  We should do a search (for "test") and re-enter
  // search mode.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  // Switch to tab 2.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  UrlbarTestUtils.assertSearchMode(window, null);

  // Do another search (in tab 2) and enter search mode.  Use a different source
  // from tab 0 just to use something different.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test tab 2",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.enterSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.TABS,
  });

  // Switch back to tab 0.  We should do a search and still be in search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  // Switch to tab 1.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 2.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.TABS,
  });

  // Exit search mode.
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });

  // Switch to tab 0.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  // Switch back to tab 2.  We should do a search but search mode should be
  // inactive.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 0.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  // Exit search mode.
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });

  // Switch back to tab 2.  We should do a search but search mode should be
  // inactive.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 0.  We should do a search but search mode should be
  // inactive.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, null);

  await UrlbarTestUtils.promisePopupClose(window);
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});
