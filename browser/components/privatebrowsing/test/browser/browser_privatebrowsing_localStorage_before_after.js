/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure that a storage instance used by both private and public sessions at different times does not
// allow any data to leak due to cached values.

// Step 1: Load browser_privatebrowsing_localStorage_before_after_page.html in a private tab, causing a storage
//   item to exist. Close the tab.
// Step 2: Load the same page in a non-private tab, ensuring that the storage instance reports only one item
//   existing.

add_task(function test() {
  let testURI = "about:blank";
  let prefix = 'http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/';

  // Step 1.
  let privateWin = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  let privateBrowser = privateWin.gBrowser.addTab(
    prefix + 'browser_privatebrowsing_localStorage_before_after_page.html').linkedBrowser;
  yield BrowserTestUtils.browserLoaded(privateBrowser);

  is(privateBrowser.contentTitle, '1', "localStorage should contain 1 item");

  // Step 2.
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let browser = win.gBrowser.addTab(
    prefix + 'browser_privatebrowsing_localStorage_before_after_page2.html').linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);

  is(browser.contentTitle, 'null|0', 'localStorage should contain 0 items');

  // Cleanup
  yield BrowserTestUtils.closeWindow(privateWin);
  yield BrowserTestUtils.closeWindow(win);
});
