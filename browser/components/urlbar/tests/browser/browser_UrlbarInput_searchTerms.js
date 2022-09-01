/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These tests check the general behavior of the Urlbar
// when search terms are shown in the Urlbar.

let originalEngine, defaultTestEngine, mochiTestEngine;

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

  await SearchTestUtils.installSearchExtension({
    name: "MochiSearch",
    search_url: "https://mochi.test:8888/",
    search_url_get_params: "q={searchTerms}&pc=fake_code",
  });
  mochiTestEngine = Services.search.getEngineByName("MochiSearch");

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

// If a SERP uses the History API to modify the URI,
// the search term, will still show up in the URL bar.
add_task(async function history_push_state() {
  let { tab } = await searchWithTab(SEARCH_STRING);

  let locationChangePromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    let url = new URL(content.window.location);
    url.searchParams.set("pc", "fake_code_2");
    content.history.pushState({}, "", url);
  });

  await locationChangePromise;
  // Check URI to make sure that it's actually been changed
  Assert.equal(
    gBrowser.currentURI.spec,
    `https://www.example.com/?q=chocolate+cake&pc=fake_code_2`,
    "URI of Urlbar should have changed"
  );

  Assert.equal(
    gURLBar.value,
    SEARCH_STRING,
    `Search string ${SEARCH_STRING} should be in the url bar`
  );

  BrowserTestUtils.removeTab(tab);
});

// When a user changes their default search engine, and does
// a search with it again using the one-off search, they should
// no longer see the search term in the URL bar for that engine.
// If they change it back, they should see the search term in
// the url bar.
add_task(async function non_default_search() {
  let { tab } = await searchWithTab(SEARCH_STRING);

  await Services.search.setDefault(mochiTestEngine);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: SEARCH_STRING,
  });

  let [expectedSearchUrl] = UrlbarUtils.getSearchQueryUrl(
    defaultTestEngine,
    SEARCH_STRING
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    expectedSearchUrl
  );

  await UrlbarTestUtils.enterSearchMode(window, {
    engineName: defaultTestEngine.name,
  });
  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter");
  await browserLoadedPromise;

  Assert.equal(gURLBar.value, expectedSearchUrl, `URL should be in URL bar`);

  await Services.search.setDefault(defaultTestEngine);
  await searchWithTab(SEARCH_STRING, tab);

  BrowserTestUtils.removeTab(tab);
});
