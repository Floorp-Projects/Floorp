/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test() {
  requestLongerTimeout(2);
  const page1 =
    "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/" +
    "browser_privatebrowsing_localStorage_page1.html";

  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  win.gBrowser.selectedTab = BrowserTestUtils.addTab(win.gBrowser, page1);
  let browser = win.gBrowser.selectedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  BrowserTestUtils.loadURI(
    browser,
    "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/" +
      "browser_privatebrowsing_localStorage_page2.html"
  );
  await BrowserTestUtils.browserLoaded(browser);

  is(browser.contentTitle, "2", "localStorage should contain 2 items");

  // Cleanup
  await BrowserTestUtils.closeWindow(win);
});
