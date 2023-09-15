/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is tests for the default titles that new bookmarks get.

var tests = [
  // Common page.
  {
    url: "http://example.com/browser/browser/components/places/tests/browser/dummy_page.html",
    title: "Dummy test page",
    isError: false,
  },
  // Data URI.
  {
    url: "data:text/html;charset=utf-8,<title>test%20data:%20url</title>",
    title: "test data: url",
    isError: false,
  },
  // about:neterror
  {
    url: "data:application/xhtml+xml,",
    title: "data:application/xhtml+xml,",
    isError: true,
  },
  // about:certerror
  {
    url: "https://untrusted.example.com/somepage.html",
    title: "https://untrusted.example.com/somepage.html",
    isError: true,
  },
];

SpecialPowers.pushPrefEnv({
  set: [["browser.bookmarks.editDialog.showForNewBookmarks", false]],
});

add_task(async function check_default_bookmark_title() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://www.example.com/"
  );
  let browser = tab.linkedBrowser;

  // Test that a bookmark of each URI gets the corresponding default title.
  for (let { url, title, isError } of tests) {
    let promiseLoaded = BrowserTestUtils.browserLoaded(
      browser,
      false,
      url,
      isError
    );
    BrowserTestUtils.startLoadingURIString(browser, url);
    await promiseLoaded;

    await checkBookmark(url, title);
  }

  // Network failure test: now that dummy_page.html is in history, bookmarking
  // it should give the last known page title as the default bookmark title.

  // Simulate a network outage with offline mode. (Localhost is still
  // accessible in offline mode, so disable the test proxy as well.)
  BrowserOffline.toggleOfflineStatus();
  let proxy = Services.prefs.getIntPref("network.proxy.type");
  Services.prefs.setIntPref("network.proxy.type", 0);
  registerCleanupFunction(function () {
    BrowserOffline.toggleOfflineStatus();
    Services.prefs.setIntPref("network.proxy.type", proxy);
  });

  // LOAD_FLAGS_BYPASS_CACHE isn't good enough. So clear the cache.
  Services.cache2.clear();

  let { url, title } = tests[0];

  let promiseLoaded = BrowserTestUtils.browserLoaded(
    browser,
    false,
    null,
    true
  );
  BrowserTestUtils.startLoadingURIString(browser, url);
  await promiseLoaded;

  // The offline mode test is only good if the page failed to load.
  await SpecialPowers.spawn(browser, [], function () {
    Assert.equal(
      content.document.documentURI.substring(0, 14),
      "about:neterror",
      "Offline mode successfully simulated network outage."
    );
  });
  await checkBookmark(url, title);

  BrowserTestUtils.removeTab(tab);
});

// Bookmark the current page and confirm that the new bookmark has the expected
// title. (Then delete the bookmark.)
async function checkBookmark(url, expected_title) {
  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    url,
    "Trying to bookmark the expected uri"
  );

  let promiseBookmark = PlacesTestUtils.waitForNotification(
    "bookmark-added",
    events =>
      events.some(
        ({ url: eventUrl }) =>
          eventUrl == gBrowser.selectedBrowser.currentURI.spec
      )
  );
  PlacesCommandHook.bookmarkPage();
  await promiseBookmark;

  let bookmark = await PlacesUtils.bookmarks.fetch({ url });

  Assert.ok(bookmark, "Found the expected bookmark");
  Assert.equal(
    bookmark.title,
    expected_title,
    "Bookmark got a good default title."
  );

  await PlacesUtils.bookmarks.remove(bookmark);
}
