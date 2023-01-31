/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

async function doTest(isPrivate) {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: isPrivate });
  let defaultURL = AboutNewTab.newTabURL;
  let newTabURL;
  let mode;
  let testURL = "https://example.com/";
  if (isPrivate) {
    mode = "per window private browsing";
    newTabURL = "about:privatebrowsing";
  } else {
    mode = "normal";
    newTabURL = "about:newtab";
  }

  await openNewTab(win, newTabURL);
  // Check the new tab opened while in normal/private mode
  is(
    win.gBrowser.selectedBrowser.currentURI.spec,
    newTabURL,
    "URL of NewTab should be " + newTabURL + " in " + mode + " mode"
  );

  // Set the custom newtab url
  AboutNewTab.newTabURL = testURL;
  is(AboutNewTab.newTabURL, testURL, "Custom newtab url is set");

  // Open a newtab after setting the custom newtab url
  await openNewTab(win, testURL);
  is(
    win.gBrowser.selectedBrowser.currentURI.spec,
    testURL,
    "URL of NewTab should be the custom url"
  );

  // Clear the custom url.
  AboutNewTab.resetNewTabURL();
  is(AboutNewTab.newTabURL, defaultURL, "No custom newtab url is set");

  win.gBrowser.removeTab(win.gBrowser.selectedTab);
  win.gBrowser.removeTab(win.gBrowser.selectedTab);
  await BrowserTestUtils.closeWindow(win);
}

add_task(async function test_newTabService() {
  // check whether any custom new tab url has been configured
  ok(!AboutNewTab.newTabURLOverridden, "No custom newtab url is set");

  // test normal mode
  await doTest(false);

  // test private mode
  await doTest(true);
});

async function openNewTab(aWindow, aExpectedURL) {
  // Open a new tab
  aWindow.BrowserOpenTab();
  let browser = aWindow.gBrowser.selectedBrowser;

  // We're already loaded.
  if (browser.currentURI.spec === aExpectedURL) {
    return;
  }

  // Wait for any location change.
  await BrowserTestUtils.waitForLocationChange(aWindow.gBrowser);
}
