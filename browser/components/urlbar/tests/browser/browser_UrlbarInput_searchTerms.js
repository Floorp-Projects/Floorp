/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These tests check the behavior of the Urlbar when loading a page
// whose url matches that of the default search engine.

let originalEngine, defaultTestEngine;

// The main search string used in tests
const SEARCH_STRING = "chocolate cake";

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.widget.inNavBar", false],
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
  });
});

// Starts a search with a tab and asserts that
// the state of the Urlbar contains the search term
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

function assertSearchStringIsInUrlbar(searchString) {
  Assert.equal(
    gURLBar.value,
    searchString,
    `Search string ${searchString} should be in the url bar`
  );
  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "invalid",
    "Pageproxystate should be invalid"
  );
}

// Search terms should show up in the url bar if the pref is on
// and the SERP url matches the one constructed in Firefox
add_task(async function list_of_search_strings() {
  const searches = [
    {
      // Single word
      searchString: "chocolate",
    },
    {
      // Word with space
      searchString: "chocolate cake",
    },
    {
      // Special characters
      searchString: "chocolate;,?:@&=+$-_.!~*'()#cake",
    },
    {
      searchString: '"chocolate cake" -recipes',
    },
    {
      // Search with special characters
      searchString: "site:example.com chocolate -cake",
    },
  ];

  for (let { searchString } of searches) {
    let { tab } = await searchWithTab(searchString);
    BrowserTestUtils.removeTab(tab);
  }
});

// If a user does a search, goes to another page, and then
// goes back to the SERP, the search term should show.
add_task(async function go_back() {
  let { tab } = await searchWithTab(SEARCH_STRING);

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  BrowserTestUtils.loadURI(
    tab.linkedBrowser,
    "http://www.example.com/some_url"
  );
  await browserLoadedPromise;

  let pageShowPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "pageshow"
  );
  tab.linkedBrowser.goBack();
  await pageShowPromise;

  assertSearchStringIsInUrlbar(SEARCH_STRING);

  BrowserTestUtils.removeTab(tab);
});

// Manually loading a url that matches a search query url
// should show the search term in the Urlbar.
add_task(async function load_url() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let [expectedSearchUrl] = UrlbarUtils.getSearchQueryUrl(
    defaultTestEngine,
    SEARCH_STRING
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    expectedSearchUrl
  );
  BrowserTestUtils.loadURI(tab.linkedBrowser, expectedSearchUrl);
  await browserLoadedPromise;
  assertSearchStringIsInUrlbar(SEARCH_STRING);

  BrowserTestUtils.removeTab(tab);
});
