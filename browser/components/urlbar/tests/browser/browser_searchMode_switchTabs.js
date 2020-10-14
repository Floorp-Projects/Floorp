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

// Enters search mode using the one-off buttons.
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
  await UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 0.  We should do a search (for "test") and re-enter
  // search mode.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    entry: "oneoff",
  });
  Assert.equal(
    gURLBar.value,
    "test",
    "Value should remain the search string after switching back"
  );

  // Switch to tab 2.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await UrlbarTestUtils.assertSearchMode(window, null);

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
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    entry: "oneoff",
  });
  Assert.equal(
    gURLBar.value,
    "test",
    "Value should remain the search string after switching back"
  );

  // Switch to tab 1.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);
  await UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 2.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.TABS,
    entry: "oneoff",
  });
  Assert.equal(
    gURLBar.value,
    "test tab 2",
    "Value should remain the search string after switching back"
  );

  // Exit search mode.
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });

  // Switch to tab 0.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    entry: "oneoff",
  });
  Assert.equal(
    gURLBar.value,
    "test",
    "Value should remain the search string after switching back"
  );

  // Switch back to tab 2.  We should do a search but search mode should be
  // inactive.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.value,
    "test tab 2",
    "Value should remain the search string after switching back"
  );

  // Switch back to tab 0.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    entry: "oneoff",
  });
  Assert.equal(
    gURLBar.value,
    "test",
    "Value should remain the search string after switching back"
  );

  // Exit search mode.
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });

  // Switch back to tab 2.  We should do a search but search mode should be
  // inactive.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.value,
    "test tab 2",
    "Value should remain the search string after switching back"
  );

  // Switch back to tab 0.  We should do a search but search mode should be
  // inactive.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.value,
    "test",
    "Value should remain the search string after switching back"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

// Enters search mode by typing a restriction char with no search string.
// Search mode and the search string should be restored after switching back to
// the tab.
add_task(async function userTypedValue_empty() {
  await doUserTypedValueTest("");
});

// Enters search mode by typing a restriction char followed by a search string.
// Search mode and the search string should be restored after switching back to
// the tab.
add_task(async function userTypedValue_nonEmpty() {
  await doUserTypedValueTest("foo bar");
});

/**
 * Enters search mode by typing a restriction char followed by a search string,
 * opens a new tab and immediately closes it so we switch back to the search
 * mode tab, and checks the search mode state and input value.
 *
 * @param {string} searchString
 *   The search string to enter search mode with.
 */
async function doUserTypedValueTest(searchString) {
  let value = `${UrlbarTokenizer.RESTRICT.BOOKMARK} ${searchString}`;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value,
    fireInputEvent: true,
  });
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    entry: "typed",
  });
  Assert.equal(
    gURLBar.value,
    searchString,
    "Sanity check: Value is the search string"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab({ gBrowser });
  BrowserTestUtils.removeTab(tab);

  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    entry: "typed",
  });
  Assert.equal(
    gURLBar.value,
    searchString,
    "Value should remain the search string after switching back"
  );

  gURLBar.handleRevert();
}
