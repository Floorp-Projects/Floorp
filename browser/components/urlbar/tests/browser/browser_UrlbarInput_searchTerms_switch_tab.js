/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These tests check the behavior of the Urlbar when search terms are shown
// and the user switches between tabs.

let defaultTestEngine;

// The main search keyword used in tests
const SEARCH_STRING = "chocolate cake";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  await SearchTestUtils.installSearchExtension(
    {
      name: "MozSearch",
      search_url: "https://www.example.com/",
      search_url_get_params: "q={searchTerms}&pc=fake_code",
    },
    { setAsDefault: true }
  );
  defaultTestEngine = Services.search.getEngineByName("MozSearch");

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
  });
});

async function searchWithTab(
  searchString,
  tab = null,
  engine = defaultTestEngine
) {
  if (!tab) {
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  }

  let [expectedSearchUrl] = UrlbarUtils.getSearchQueryUrl(engine, searchString);
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    expectedSearchUrl
  );

  gURLBar.focus();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: searchString,
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_Enter");
  await browserLoadedPromise;

  assertSearchStringIsInUrlbar(searchString);

  return { tab, expectedSearchUrl };
}

// Users should be able to search, change the tab, and come
// back to the original tab to see the search term again
add_task(async function change_tab() {
  let { tab: tab1 } = await searchWithTab(SEARCH_STRING);
  let { tab: tab2 } = await searchWithTab("another keyword");
  let { tab: tab3 } = await searchWithTab("yet another keyword");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  assertSearchStringIsInUrlbar(SEARCH_STRING);

  await BrowserTestUtils.switchTab(gBrowser, tab2);
  assertSearchStringIsInUrlbar("another keyword");

  await BrowserTestUtils.switchTab(gBrowser, tab3);
  assertSearchStringIsInUrlbar("yet another keyword");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});

// If a user types in the URL bar, and the user goes to a
// different tab, the original tab should still contain the
// text written by the user.
add_task(async function user_overwrites_search_term() {
  let { tab: tab1 } = await searchWithTab(SEARCH_STRING);

  gURLBar.focus();
  gURLBar.select();
  EventUtils.sendString("another_word");

  Assert.notEqual(
    gURLBar.value,
    SEARCH_STRING,
    `Search string ${SEARCH_STRING} should not be in the url bar`
  );

  // Open a new tab, switch back to the first and
  // check that the user typed value is still there.
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.switchTab(gBrowser, tab1);

  Assert.equal(
    gURLBar.value,
    "another_word",
    "another_word should be in the url bar"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// If a user clears the URL bar, and goes to a different tab,
// and returns to the initial tab, it should show the search term again.
add_task(async function user_overwrites_search_term() {
  let { tab: tab1 } = await searchWithTab(SEARCH_STRING);

  gURLBar.focus();
  gURLBar.select();
  EventUtils.sendKey("delete");

  Assert.equal(gURLBar.value, "", "Empty string should be in url bar.");

  // Open a new tab, switch back to the first and check
  // the blank string is replaced with the search string.
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.switchTab(gBrowser, tab1);

  assertSearchStringIsInUrlbar(SEARCH_STRING, {
    pageProxyState: "invalid",
    userTypedValue: "",
  });

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
