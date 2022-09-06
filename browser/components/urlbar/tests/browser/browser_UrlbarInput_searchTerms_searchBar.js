/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These tests check the behavior of the Urlbar when a user enables
// the search bar and showSearchTerms is true.

let defaultTestEngine, originalEngine;

const { CustomizableUITestUtils } = ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm"
);

const gCUITestUtils = new CustomizableUITestUtils(window);
const SEARCH_STRING = "example_string";

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.widget.inNavBar", true],
      ["browser.urlbar.showSearchTerms", true],
    ],
  });

  await SearchTestUtils.installSearchExtension({
    name: "MozSearch",
    search_url: "https://www.example.com/",
    search_url_get_params: "q={searchTerms}&pc=fake_code",
  });
  defaultTestEngine = Services.search.getEngineByName("MozSearch");

  originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(defaultTestEngine);

  registerCleanupFunction(async function() {
    await Services.search.setDefault(originalEngine);
    await PlacesUtils.history.clear();
    gCUITestUtils.removeSearchBar();
  });
});

function assertSearchStringIsNotInUrlbar(searchString) {
  Assert.notEqual(
    gURLBar.value,
    searchString,
    `Search string ${searchString} should not be in the url bar`
  );
  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "valid",
    "Pageproxystate should be valid"
  );
}

// When a user enables the search bar, and does a search in the search bar,
// the search term should not show in the URL bar.
add_task(async function search_bar_on() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await gCUITestUtils.addSearchBar();

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    `https://www.example.com/?q=${SEARCH_STRING}&pc=fake_code`
  );

  let searchBar = BrowserSearch.searchBar;
  searchBar.value = SEARCH_STRING;
  searchBar.focus();
  EventUtils.synthesizeKey("KEY_Enter");

  await browserLoadedPromise;
  assertSearchStringIsNotInUrlbar(SEARCH_STRING);

  BrowserTestUtils.removeTab(tab);
});

// When a user enables the search bar, and does a search in the URL bar,
// the search term should still not show in the URL bar.
add_task(async function search_bar_on_with_url_bar_search() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await gCUITestUtils.addSearchBar();

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    `https://www.example.com/?q=${SEARCH_STRING}&pc=fake_code`
  );

  gURLBar.focus();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: SEARCH_STRING,
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_Enter");

  await browserLoadedPromise;
  assertSearchStringIsNotInUrlbar(SEARCH_STRING);

  BrowserTestUtils.removeTab(tab);
});
