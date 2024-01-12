/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks whether certain patterns of search terms won't show
// in the Urlbar as a search term.

// Can regularly cause a timeout error on Mac verify mode.
requestLongerTimeout(5);

ChromeUtils.defineESModuleGetters(this, {
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

let defaultTestEngine, tab;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  await SearchTestUtils.installSearchExtension(
    {
      name: "MozSearch",
      search_url: "https://www.example.com/",
      search_url_get_params: "q={searchTerms}",
    },
    { setAsDefault: true }
  );
  defaultTestEngine = Services.search.getEngineByName("MozSearch");

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
  });
});

async function checkSearchString(searchString, isIpv6) {
  info("Search for term:", searchString);
  let [searchUrl] = UrlbarUtils.getSearchQueryUrl(
    defaultTestEngine,
    searchString
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    searchUrl
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, searchUrl);
  await browserLoadedPromise;

  // decodeURI is necessary for matching square brackets in IPV6.
  let expectedUrl = isIpv6 ? decodeURI(searchUrl) : searchUrl;

  if (UrlbarPrefs.get("trimHttps") && expectedUrl.startsWith("https://")) {
    expectedUrl = expectedUrl.slice("https://".length);
  }

  Assert.equal(gURLBar.value, expectedUrl, "The full URL should be in URL bar");
  Assert.equal(
    gBrowser.userTypedValue,
    null,
    `There should not be a userTypedValue for ${searchString}`
  );
  Assert.equal(
    gBrowser.selectedBrowser.searchTerms,
    "",
    "searchTerms should be empty."
  );
}

add_task(async function unsafe_search_strings() {
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  const searches = [
    "example.org",
    "www.example.org",
    "  www.example.org  ",
    "www.example.org/path",
    "https://",
    "https://example",
    "https://example.org",
    "https://example.org/path",
    "https:// example.org/",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://",
    "http://example",
    "http://example.org",
    "http://example.org/path",
    "http:// example.org/path",
    "file://example",
    // Some protocols can be fixed up.
    "ttp://example",
    "htp://example",
    "ttps://example",
    "tps://example",
    "ps://example",
    "htps://example",
    // Protocol fixup with a space and path.
    "ttp:// example.org/path",
    "htp:// example.org/path",
    "ttps:// example.org/path",
    "tps:// example.org/path",
    "ps:// example.org/path",
    "htps:// example.org/path",
    // Variations of spaces.
    "https ://example.org",
    "https: //example.org",
    "https:/ /example.org",
    "https://\texample.org",
    "https://\r\nexample.org",
    // URL without protocols.
    "www.example.org",
    "www.example.org/path",
    "www.example.org/path path",
    "www. example.org/path",
    // Long string exceeding threshold.
    "h".repeat(UrlbarUtils.MAX_TEXT_LENGTH + 1),
  ];
  for (let searchString of searches) {
    await checkSearchString(searchString, false);
  }

  const ipV6Searches = [
    "[2001:db8:85a3:8d3:1319:8a2e:370:7348]/example",
    // Includes a space.
    "[2001:db8:85a3:8d3:1319:8a2e:370:7348]/path path",
  ];
  for (let searchString of ipV6Searches) {
    await checkSearchString(searchString, true);
  }

  BrowserTestUtils.removeTab(tab);
});
