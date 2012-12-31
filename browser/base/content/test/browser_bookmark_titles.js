/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is tests for the default titles that new bookmarks get.

let tests = [
    ['http://example.com/browser/browser/base/content/test/dummy_page.html',
     'Dummy test page'],
    ['data:text/html;charset=utf-8,<title>test data: url</title>',
     'test data: url'],
    ['http://unregistered-domain.example',
     'http://unregistered-domain.example/'],
    ['https://untrusted.example.com/somepage.html',
     'https://untrusted.example.com/somepage.html']
];

function generatorTest() {
    gBrowser.selectedTab = gBrowser.addTab();
    let browser = gBrowser.selectedBrowser;

    browser.addEventListener("DOMContentLoaded", nextStep, true);
    registerCleanupFunction(function () {
        browser.removeEventListener("DOMContentLoaded", nextStep, true);
        gBrowser.removeCurrentTab();
    });

    yield; // Wait for the new tab to load.

    // Test that a bookmark of each URI gets the corresponding default title.
    for (let i = 0; i < tests.length; ++i) {
        let [uri, title] = tests[i];
        content.location = uri;
        yield;
        checkBookmark(uri, title);
    }

    // Network failure test: now that dummy_page.html is in history, bookmarking
    // it should give the last known page title as the default bookmark title.

    // Simulate a network outage with offline mode. (Localhost is still
    // accessible in offline mode, so disable the test proxy as well.)
    BrowserOffline.toggleOfflineStatus();
    let proxy = Services.prefs.getIntPref('network.proxy.type');
    Services.prefs.setIntPref('network.proxy.type', 0);
    registerCleanupFunction(function () {
        BrowserOffline.toggleOfflineStatus();
        Services.prefs.setIntPref('network.proxy.type', proxy);
    });

    // LOAD_FLAGS_BYPASS_CACHE isn't good enough. So clear the cache.
    Services.cache.evictEntries(Services.cache.STORE_ANYWHERE);

    let [uri, title] = tests[0];
    content.location = uri;
    yield;
    // The offline mode test is only good if the page failed to load.
    is(content.document.documentURI.substring(0, 14), 'about:neterror',
        "Offline mode successfully simulated network outage.");
    checkBookmark(uri, title);
}

// Bookmark the current page and confirm that the new bookmark has the expected
// title. (Then delete the bookmark.)
function checkBookmark(uri, expected_title) {
    PlacesCommandHook.bookmarkCurrentPage(false);
    
    let id = PlacesUtils.getMostRecentBookmarkForURI(PlacesUtils._uri(uri));
    let title = PlacesUtils.bookmarks.getItemTitle(id);

    is(title, expected_title, "Bookmark got a good default title.");

    PlacesUtils.bookmarks.removeItem(id);
}

