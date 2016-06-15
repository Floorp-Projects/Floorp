/**
 * Test for Bug 446171 - Name field of bookmarks saved via 'Bookmark All Tabs'
 * has '(null)' value if history is disabled or just in private browsing mode
 */
"use strict"

add_task(function* () {
  const BASE_URL = "http://example.org/browser/browser/components/places/tests/browser/";
  const TEST_PAGES = [
    BASE_URL + "bookmark_dummy_1.html",
    BASE_URL + "bookmark_dummy_2.html",
    BASE_URL + "bookmark_dummy_1.html"
  ];

  function promiseAddTab(url) {
    return BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  }

  let tabs = yield Promise.all(TEST_PAGES.map(promiseAddTab));

  let URIs = PlacesCommandHook.uniqueCurrentPages;
  is(URIs.length, 3, "Only unique pages are returned");

  Assert.deepEqual(URIs.map(URI => URI.uri.spec), [
    "about:blank",
    BASE_URL + "bookmark_dummy_1.html",
    BASE_URL + "bookmark_dummy_2.html"
  ], "Correct URIs are returned");

  Assert.deepEqual(URIs.map(URI => URI.title), [
    "New Tab", "Bookmark Dummy 1", "Bookmark Dummy 2"
  ], "Correct titles are returned");

  registerCleanupFunction(function* () {
    yield Promise.all(tabs.map(BrowserTestUtils.removeTab));
  });
});
