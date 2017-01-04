/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is tests for the default titles that new bookmarks get.

var tests = [
    // Common page.
    ['http://example.com/browser/browser/base/content/test/general/dummy_page.html',
     'Dummy test page'],
    // Data URI.
    ['data:text/html;charset=utf-8,<title>test%20data:%20url</title>',
     'test data: url'],
    // about:neterror
    ['data:application/vnd.mozilla.xul+xml,',
     'data:application/vnd.mozilla.xul+xml,'],
    // about:certerror
    ['https://untrusted.example.com/somepage.html',
     'https://untrusted.example.com/somepage.html']
];

add_task(function* () {
    gBrowser.selectedTab = gBrowser.addTab();
    let browser = gBrowser.selectedBrowser;
    browser.stop(); // stop the about:blank load.

    // Test that a bookmark of each URI gets the corresponding default title.
    for (let i = 0; i < tests.length; ++i) {
        let [uri, title] = tests[i];

        let promiseLoaded = promisePageLoaded(browser);
        BrowserTestUtils.loadURI(browser, uri);
        yield promiseLoaded;
        yield checkBookmark(uri, title);
    }

    // Network failure test: now that dummy_page.html is in history, bookmarking
    // it should give the last known page title as the default bookmark title.

    // Simulate a network outage with offline mode. (Localhost is still
    // accessible in offline mode, so disable the test proxy as well.)
    BrowserOffline.toggleOfflineStatus();
    let proxy = Services.prefs.getIntPref('network.proxy.type');
    Services.prefs.setIntPref('network.proxy.type', 0);
    registerCleanupFunction(function() {
        BrowserOffline.toggleOfflineStatus();
        Services.prefs.setIntPref('network.proxy.type', proxy);
    });

    // LOAD_FLAGS_BYPASS_CACHE isn't good enough. So clear the cache.
    Services.cache2.clear();

    let [uri, title] = tests[0];

    let promiseLoaded = promisePageLoaded(browser);
    BrowserTestUtils.loadURI(browser, uri);
    yield promiseLoaded;

    // The offline mode test is only good if the page failed to load.
    yield ContentTask.spawn(browser, null, function() {
      is(content.document.documentURI.substring(0, 14), 'about:neterror',
          "Offline mode successfully simulated network outage.");
    });
    yield checkBookmark(uri, title);

    gBrowser.removeCurrentTab();
});

// Bookmark the current page and confirm that the new bookmark has the expected
// title. (Then delete the bookmark.)
function* checkBookmark(uri, expected_title) {
    is(gBrowser.selectedBrowser.currentURI.spec, uri,
       "Trying to bookmark the expected uri");

    let promiseBookmark = promiseOnBookmarkItemAdded(gBrowser.selectedBrowser.currentURI);
    PlacesCommandHook.bookmarkCurrentPage(false);
    yield promiseBookmark;

    let id = PlacesUtils.getMostRecentBookmarkForURI(PlacesUtils._uri(uri));
    ok(id > 0, "Found the expected bookmark");
    let title = PlacesUtils.bookmarks.getItemTitle(id);
    is(title, expected_title, "Bookmark got a good default title.");

    PlacesUtils.bookmarks.removeItem(id);
}

// BrowserTestUtils.browserLoaded doesn't work for the about pages, so use a
// custom page load listener.
function promisePageLoaded(browser) {
  return ContentTask.spawn(browser, null, function* () {
    yield ContentTaskUtils.waitForEvent(this, "DOMContentLoaded", true,
        (event) => {
          return event.originalTarget === content.document &&
                 event.target.location.href !== "about:blank"
        });
  });
}
