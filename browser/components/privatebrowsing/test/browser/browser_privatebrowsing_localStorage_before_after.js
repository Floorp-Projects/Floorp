/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure that a storage instance used by both private and public sessions at different times does not
// allow any data to leak due to cached values.

// Step 1: Load browser_privatebrowsing_localStorage_before_after_page.html in a private tab, causing a storage
//   item to exist. Close the tab.
// Step 2: Load the same page in a non-private tab, ensuring that the storage instance reports only one item
//   existing.

add_task(async function test() {
  let prefix = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/";

  // Step 1.
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let privateBrowser = BrowserTestUtils.addTab(privateWin.gBrowser,
    prefix + "browser_privatebrowsing_localStorage_before_after_page.html").linkedBrowser;
  await BrowserTestUtils.browserLoaded(privateBrowser);

  is(privateBrowser.contentTitle, "1", "localStorage should contain 1 item");

  // Step 2.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let browser = BrowserTestUtils.addTab(win.gBrowser,
    prefix + "browser_privatebrowsing_localStorage_before_after_page2.html").linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  is(browser.contentTitle, "null|0", "localStorage should contain 0 items");

  // Cleanup
  await BrowserTestUtils.closeWindow(privateWin);
  await BrowserTestUtils.closeWindow(win);
});
