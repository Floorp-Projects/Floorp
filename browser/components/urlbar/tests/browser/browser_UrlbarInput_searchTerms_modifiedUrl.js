/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These tests check the behavior of the Urlbar when search terms are
// expected to be shown but the url is modified from what the browser expects.

let defaultTestEngine;

// The main search string used in tests
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

// If a SERP uses the History API to modify the URI,
// the search term should still show in the URL bar.
add_task(async function history_push_state() {
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
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, expectedSearchUrl);
  await browserLoadedPromise;

  let locationChangePromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
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

// Loading a url that looks like a search query url but has additional
// query params should not show the search term in the Urlbar.
add_task(async function url_with_additional_query_params() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let [expectedSearchUrl] = UrlbarUtils.getSearchQueryUrl(
    defaultTestEngine,
    SEARCH_STRING
  );
  // Add a query param
  expectedSearchUrl += "&another_code=something_else";
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    expectedSearchUrl
  );
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, expectedSearchUrl);
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

  BrowserTestUtils.removeTab(tab);
});
