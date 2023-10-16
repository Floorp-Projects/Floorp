/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These tests check the behavior of the Urlbar when using search mode

let defaultTestEngine;

// The main search string used in tests
const SEARCH_STRING = "chocolate cake";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  await SearchTestUtils.installSearchExtension({
    name: "MozSearch",
    search_url: "https://www.example.com/",
    search_url_get_params: "q={searchTerms}&pc=fake_code",
  });
  defaultTestEngine = Services.search.getEngineByName("MozSearch");

  await SearchTestUtils.installSearchExtension(
    {
      name: "MochiSearch",
      search_url: "https://mochi.test:8888/",
      search_url_get_params: "q={searchTerms}&pc=fake_code",
    },
    { setAsDefault: true }
  );

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
  });
});

// When a user does a search with search mode, they should
// not see the search term in the URL bar for that engine.
add_task(async function non_default_search() {
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

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: SEARCH_STRING,
  });
  await UrlbarTestUtils.enterSearchMode(window, {
    engineName: defaultTestEngine.name,
  });
  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter");
  await browserLoadedPromise;

  Assert.equal(
    gURLBar.value,
    UrlbarTestUtils.trimURL(expectedSearchUrl),
    `URL should be in URL bar`
  );
  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "valid",
    "Pageproxystate should be valid"
  );
  Assert.equal(
    gBrowser.userTypedValue,
    null,
    "There should not be a userTypedValue for a search on a non-default search engine"
  );
  Assert.equal(
    gBrowser.selectedBrowser.searchTerms,
    "",
    "searchTerms should be empty."
  );
  BrowserTestUtils.removeTab(tab);
});
